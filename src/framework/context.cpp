﻿/*
Distributed under the MIT License(MIT)

Copyright(c) 2023 Jihua Zou EMail: ghuazo@qq.com QQ:137336521

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files(the "Software"), to deal in the
Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and /or sell copies
of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS
OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <context.h>
#include <market_api.h>
#include <trader_api.h>
#include <interface.h>
#include <params.hpp>
#include <time_utils.hpp>
#include <process_helper.hpp>
#include "trading_section.h"
#include "price_step.h"

using namespace lt;
using namespace lt::hft ;

context::context(lifecycle_listener* lifecycle):
	_market(nullptr),
	_trader(nullptr),
	_is_runing(false),
	_realtime_thread(nullptr),
	_tick_callback(nullptr),
	_last_order_time(0),
	_bind_cpu_core(-1),
	_loop_interval(1),
	_last_tick_time(0),
	_thread_priority(0),
	_lifecycle_listener(lifecycle)
{
}
context::~context()
{
}

void context::init(const params& control_config, const params& include_config, market_api* market, trader_api* trader,bool reset_trading_day)
{
	_market = market;
	_trader = trader;
	_bind_cpu_core = control_config.get<int16_t>("bind_cpu_core");
	_loop_interval = control_config.get<uint32_t>("loop_interval");
	_thread_priority = control_config.get<int16_t>("thread_priority");
	const auto& ps_config = include_config.get<std::string>("price_step");
	_ps_config = std::make_shared<price_step>(ps_config);
	auto section_config = include_config.get<std::string>("section_config");
	_section_config = std::make_shared<trading_section>(section_config);
	int16_t process_priority = control_config.get<int16_t>("process_priority");
	if(static_cast<int16_t>(PriorityLevel::LowPriority) <= process_priority && process_priority <= static_cast<int16_t>(PriorityLevel::RealtimePriority))
	{
		PriorityLevel level = static_cast<PriorityLevel>(process_priority);
		if (!process_helper::set_priority(level))
		{
			LOG_WARNING("set_priority failed");
		}
	}
	
}

bool context::load_data()
{
	if(!_trader)
	{
		LOG_ERROR("context load trader null");
		return false;
	}
	LOG_INFO("context load trader data");
	auto trader_data = _trader->get_trader_data();
	if (!trader_data)
	{
		return false;
	}
	_position_info.clear();
	_order_info.clear();
	for (const auto& it : trader_data->orders)
	{
		auto& pos = _position_info[it.code];
		pos.id = it.code;
		if (it.offset == offset_type::OT_OPEN)
		{
			if (it.direction == direction_type::DT_LONG)
			{
				pos.long_pending += it.total_volume;
			}
			else if (it.direction == direction_type::DT_SHORT)
			{
				pos.short_pending += it.total_volume;
			}
		}
		else if (it.offset == offset_type::OT_CLSTD)
		{
			if (it.direction == direction_type::DT_LONG)
			{
				pos.today_long.frozen += it.total_volume;
			}
			else if (it.direction == direction_type::DT_SHORT)
			{
				pos.today_short.frozen += it.total_volume;
			}
		}
		else
		{
			if (it.direction == direction_type::DT_LONG)
			{
				pos.history_long.frozen += it.total_volume;
			}
			else if (it.direction == direction_type::DT_SHORT)
			{
				pos.history_short.frozen += it.total_volume;
			}

		}
		_order_info[it.estid] = it;
	}

	for (const auto& it : trader_data->positions)
	{
		auto& pos = _position_info[it.id];
		pos.id = it.id;
		pos.today_long.postion = it.today_long;
		pos.today_short.postion = it.today_short;
		pos.history_long.postion = it.history_long;
		pos.history_short.postion = it.history_short;
	}
	return true;
}

bool context::start_service()
{
	if (_is_runing)
	{
		return false;
	}

	if(!load_data())
	{
		return false ;
	}
	_is_runing = true;
	if(_trader)
	{
		_trader->bind_event(trader_event_type::TET_OrderCancel, std::bind(&context::handle_cancel, this, std::placeholders::_1));
		_trader->bind_event(trader_event_type::TET_OrderPlace, std::bind(&context::handle_entrust, this, std::placeholders::_1));
		_trader->bind_event(trader_event_type::TET_OrderDeal, std::bind(&context::handle_deal, this, std::placeholders::_1));
		_trader->bind_event(trader_event_type::TET_OrderTrade, std::bind(&context::handle_trade, this, std::placeholders::_1));
		_trader->bind_event(trader_event_type::TET_OrderError, std::bind(&context::handle_error, this, std::placeholders::_1));
	}
	if(_market)
	{
		_market->bind_event(market_event_type::MET_TickReceived, std::bind(&context::handle_tick, this, std::placeholders::_1));
	}
	
	_realtime_thread = new std::thread([this]()->void{
		if(0 <= _bind_cpu_core && _bind_cpu_core < static_cast<int16_t>(std::thread::hardware_concurrency()))
		{
			if (!process_helper::thread_bind_core(static_cast<uint32_t>(_bind_cpu_core)))
			{
				LOG_WARNING("bind to core failed :", _bind_cpu_core);
			}
		}
		if (static_cast<int16_t>(PriorityLevel::LowPriority) <= _thread_priority && _thread_priority <= static_cast<int16_t>(PriorityLevel::RealtimePriority))
		{
			PriorityLevel level = static_cast<PriorityLevel>(_thread_priority);
			if (!process_helper::set_thread_priority(level))
			{
				LOG_WARNING("set_thread_priority failed");
			}
		}
		
		check_crossday();
		if(_lifecycle_listener)
		{
			_lifecycle_listener->on_init();
		}
		while (_is_runing/* || !_trader->is_idle()*/)
		{
			auto begin = std::chrono::system_clock::now();
			this->update();
			auto use_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - begin);
			auto duration = std::chrono::microseconds(_loop_interval);
			if (use_time < duration)
			{
				std::this_thread::sleep_for(duration - use_time);
			}
		}
		if (_lifecycle_listener)
		{
			_lifecycle_listener->on_destroy();
		}
	});
	return true ;
}

void context::update()
{
	if (_market)
	{
		_market->update();
	}
	if(is_in_trading())
	{
		if (_trader)
		{
			_trader->update();
		}
		if (_lifecycle_listener)
		{
			_lifecycle_listener->on_update();
		}
		this->check_condition();
	}
}

bool context::stop_service()
{
	if(!_is_runing)
	{
		return false;
	}
	_is_runing = false ;
	if(_realtime_thread)
	{
		_realtime_thread->join();
		delete _realtime_thread;
		_realtime_thread = nullptr;
	}
	if(_trader)
	{
		_trader->clear_event();
	}
	if(_market)
	{
		_market->clear_event();
	}
	return true ;
}


order_statistic context::get_all_statistic()const
{
	order_statistic all_statistic;
	for (const auto& it : _statistic_info)
	{
		 all_statistic.place_order_amount += it.second.place_order_amount;
		 all_statistic.entrust_amount += it.second.entrust_amount;
		 all_statistic.trade_amount += it.second.trade_amount;
		 all_statistic.cancel_amount += it.second.cancel_amount;
		 all_statistic.error_amount += it.second.error_amount;
	}
	return all_statistic;
}

void context::set_cancel_condition(estid_t estid, std::function<bool(estid_t)> callback)
{
	if (estid != INVALID_ESTID)
	{
		LOG_DEBUG("set_cancel_condition : ", estid);
		_need_check_condition[estid] = callback;
	}
}

const tick_info& context::get_previous_tick(const code_t& code)
{
	const auto it = _previous_tick.find(code);
	if(it == _previous_tick.end())
	{
		return default_tick;
	}
	return it->second ;
}

estid_t context::place_order(order_listener* listener, offset_type offset, direction_type direction, const code_t& code, uint32_t count, double_t price, order_flag flag)
{
	LOG_INFO("context place order : ", code.get_id(), offset, direction, price, count);
	PROFILE_DEBUG(code.get_id());
	if (!this->_trader)
	{
		LOG_ERROR("place order this->_trader null");
		return false;
	}
	if (!is_in_trading())
	{
		LOG_WARNING("place order code not in trading", code.get_id());
		return INVALID_ESTID;
	}
	PROFILE_DEBUG(code.get_id());
	if (_filter_function)
	{
		if (!_filter_function(code, offset, direction, count, price, flag))
		{
			LOG_WARNING("engine place order : _filter_function false", code.get_id(), offset, direction, price, count);
			return INVALID_ESTID;
		}
	}

	estid_t estid = this->_trader->place_order(offset, direction, code, count, price, flag);
	if (estid != INVALID_ESTID)
	{
		_order_listener[estid] = listener;
		_statistic_info[code].place_order_amount++;
	}
	PROFILE_DEBUG(code.get_id());
	return estid ;
}

bool context::cancel_order(estid_t estid)
{

	if(estid == INVALID_ESTID)
	{
		return false;
	}
	if (!this->_trader)
	{
		LOG_ERROR("cancel order this->_trader null");
		return false;
	}
	if (!is_in_trading())
	{
		LOG_WARNING("cancel order not in trading ", estid);
		return false;
	}
	LOG_INFO("context cancel_order : ", estid);
	return this->_trader->cancel_order(estid);
}

const position_info& context::get_position(const code_t& code)const
{
	const auto& it = _position_info.find(code);
	if (it != _position_info.end())
	{
		return (it->second);
	}
	return default_position;
}

const order_info& context::get_order(estid_t estid)const
{
	auto it = _order_info.find(estid);
	if (it != _order_info.end())
	{
		return (it->second);
	}
	return default_order;
}

void context::find_orders(std::vector<order_info>& order_result, std::function<bool(const order_info&)> func) const
{
	for (auto& it : _order_info)
	{
		if (func(it.second))
		{
			order_result.emplace_back(it.second);
		}
	}
}

uint32_t context::get_total_position() const
{
	uint32_t total = 0;
	for (const auto& it : _position_info)
	{
		total += it.second.get_total();
	}
	return total;
}
void context::subscribe(const std::set<code_t>& tick_data, std::function<void(const tick_info&)> tick_callback)
{
	this->_tick_callback = tick_callback;
	if(this->_market)
	{
		this->_market->subscribe(tick_data);
	}
}

void context::unsubscribe(const std::set<code_t>& tick_data)
{
	if (this->_market)
	{
		this->_market->unsubscribe(tick_data);
	}
}

daytm_t context::get_last_time()
{
	return _last_tick_time;
}

daytm_t context::last_order_time()
{
	return _last_order_time;
}

const order_statistic& context::get_order_statistic(const code_t& code)const
{
	auto it = _statistic_info.find(code);
	if(it != _statistic_info.end())
	{
		return it->second;
	}
	return default_statistic;
}


uint32_t context::get_trading_day()const
{
	if(!this->_trader)
	{
		return -0U;
	}
	return this->_trader->get_trading_day();
}

daytm_t context::get_close_time()const
{
	if(_section_config == nullptr)
	{
		LOG_FATAL("section config not init");
		return 0;
	}
	return _section_config->get_close_time();
}

bool context::is_in_trading()const
{
	if (_section_config == nullptr)
	{
		LOG_FATAL("section config not init");
		return false;
	}
	return _section_config->is_in_trading(_last_tick_time);
}



const market_info& context::get_market_info(const code_t& id)const
{
	auto it = _market_info.find(id);
	if (it == _market_info.end())
	{
		return default_market;
	}
	return it->second;
}


uint32_t context::get_total_pending()
{
	uint32_t res = 0;
	for (auto& it : _position_info)
	{
		res += (it.second.long_pending + it.second.short_pending);
	}
	return res;
}

void context::check_crossday()
{
	_last_tick_time = 0U;
	_market_info.clear();
	_statistic_info.clear();
	_last_order_time = get_last_time();
	LOG_INFO("trading ready");
}

void context::handle_entrust(const std::vector<std::any>& param)
{
	if (param.size() >= 1)
	{
		order_info order = std::any_cast<order_info>(param[0]);
		_order_info[order.estid] = (order);
		if (order.offset == offset_type::OT_OPEN)
		{
			record_pending(order.code, order.direction, order.offset, order.total_volume);
		}
		else
		{
			//平仓冻结仓位
			frozen_deduction(order.code, order.direction, order.offset, order.total_volume);
		}
		auto it = _order_listener.find(order.estid);
		if(it != _order_listener.end() && it->second)
		{
			it->second->on_entrust(order);
		}
		_last_order_time = order.create_time;
		_statistic_info[order.code].entrust_amount++;

	}
}

void context::handle_deal(const std::vector<std::any>& param)
{
	if (param.size() >= 3)
	{
		estid_t estid = std::any_cast<estid_t>(param[0]);
		uint32_t deal_volume = std::any_cast<uint32_t>(param[1]);
		uint32_t last_volume = std::any_cast<uint32_t>(param[2]);
		auto it = _order_info.find(estid);
		if (it != _order_info.end())
		{
			calculate_position(it->second.code, it->second.direction, it->second.offset, deal_volume, it->second.price);
			it->second.last_volume = last_volume;
		}
		auto listener_iter = _order_listener.find(estid);
		if (listener_iter != _order_listener.end() && listener_iter->second)
		{
			listener_iter->second->on_deal(estid, deal_volume);
		}
	}
}

void context::handle_trade(const std::vector<std::any>& param)
{
	if (param.size() >= 6)
	{

		estid_t estid = std::any_cast<estid_t>(param[0]);
		code_t code = std::any_cast<code_t>(param[1]);
		offset_type offset = std::any_cast<offset_type>(param[2]);
		direction_type direction = std::any_cast<direction_type>(param[3]);
		double_t price = std::any_cast<double_t>(param[4]);
		uint32_t trade_volume = std::any_cast<uint32_t>(param[5]);
		auto it = _order_info.find(estid);
		if (it != _order_info.end())
		{
			_order_info.erase(it);
		}
		auto listener_iter = _order_listener.find(estid);
		if (listener_iter != _order_listener.end() && listener_iter->second)
		{
			listener_iter->second->on_trade(estid, code, offset, direction, price, trade_volume);
			_order_listener.erase(listener_iter);
		}
		auto odit = _need_check_condition.find(estid);
		if (odit != _need_check_condition.end())
		{
			_need_check_condition.erase(odit);
		}
		_statistic_info[code].trade_amount++;
	}
}

void context::handle_cancel(const std::vector<std::any>& param)
{
	if (param.size() >= 7)
	{
		estid_t estid = std::any_cast<estid_t>(param[0]);
		code_t code = std::any_cast<code_t>(param[1]);
		offset_type offset = std::any_cast<offset_type>(param[2]);
		direction_type direction = std::any_cast<direction_type>(param[3]);
		double_t price = std::any_cast<double_t>(param[4]);
		uint32_t cancel_volume = std::any_cast<uint32_t>(param[5]);
		uint32_t total_volume = std::any_cast<uint32_t>(param[6]);
		auto it = _order_info.find(estid);
		if(it != _order_info.end())
		{
			//撤销解冻仓位
			if (offset == offset_type::OT_OPEN)
			{
				recover_pending(code, direction, offset, cancel_volume);
			}
			else
			{
				unfreeze_deduction(code, direction, offset, cancel_volume);
			}
			_order_info.erase(it);
		}
		auto listener_iter = _order_listener.find(estid);
		if (listener_iter != _order_listener.end() && listener_iter->second)
		{
			listener_iter->second->on_cancel(estid, code, offset, direction, price, cancel_volume, total_volume);
			_order_listener.erase(listener_iter);
		}
		auto odit = _need_check_condition.find(estid);
		if (odit != _need_check_condition.end())
		{
			_need_check_condition.erase(odit);
		}
		_statistic_info[code].cancel_amount++;
	}
}

void context::handle_tick(const std::vector<std::any>& param)
{
	
	if (param.size() >= 2)
	{
		PROFILE_DEBUG("pDepthMarketData->InstrumentID");
		auto&& last_tick = std::any_cast<const tick_info>(param[0])
		PROFILE_DEBUG(last_tick.id.get_id());
		LOG_INFO("handle_tick", last_tick.id.get_id(), last_tick.time, " ", _last_tick_time);
		if (last_tick.time > _last_tick_time)
		{
			_last_tick_time = last_tick.time;
		}
		
		auto it = _previous_tick.find(last_tick.id);
		if(it != _previous_tick.end())
		{
			tick_info& prev_tick = it->second;
			if (is_in_trading())
			{
				auto&& extend_data = std::any_cast<tick_extend>(param[1]);
				auto& current_market_info = _market_info[last_tick.id];
				current_market_info.code = last_tick.id;
				current_market_info.last_tick_info = last_tick;
				current_market_info.open_price = std::get<TEI_OPEN_PRICE>(extend_data);
				current_market_info.close_price = std::get<TEI_CLOSE_PRICE>(extend_data);
				current_market_info.standard_price = std::get<TEI_STANDARD_PRICE>(extend_data);
				current_market_info.high_price = std::get<TEI_HIGH_PRICE>(extend_data);
				current_market_info.low_price = std::get<TEI_LOW_PRICE>(extend_data);
				current_market_info.max_price = std::get<TEI_MAX_PRICE>(extend_data);
				current_market_info.min_price = std::get<TEI_MIN_PRICE>(extend_data);
				current_market_info.trading_day = last_tick.trading_day;
				current_market_info.volume_distribution[last_tick.price] += static_cast<uint32_t>(last_tick.volume - prev_tick.volume);
				if (this->_tick_callback)
				{
					PROFILE_DEBUG(last_tick.id.get_id());
					this->_tick_callback(last_tick);
				}
			}
			it->second = last_tick;
		}
		else
		{
			_previous_tick.insert(std::make_pair(last_tick.id, last_tick));
		}
	}
}

void context::handle_error(const std::vector<std::any>& param)
{
	if (param.size() >= 3)
	{
		const error_type type = std::any_cast<error_type>(param[0]);
		const estid_t estid = std::any_cast<estid_t>(param[1]);
		const uint8_t error = std::any_cast<uint8_t>(param[2]);
		
		auto it = _order_info.find(estid);
		if (it != _order_info.end())
		{
			_statistic_info[it->second.code].error_amount++;
			if (type == error_type::ET_PLACE_ORDER)
			{
				_order_info.erase(it);
			}
		}
		auto listener_iter = _order_listener.find(estid);
		if (listener_iter != _order_listener.end() && listener_iter->second)
		{
			listener_iter->second->on_error(type, estid, static_cast<error_code>(error));
			if (type == error_type::ET_PLACE_ORDER)
			{
				_order_listener.erase(listener_iter);
			}
		}
		if (type == error_type::ET_PLACE_ORDER)
		{
			auto odit = _need_check_condition.find(estid);
			if (odit != _need_check_condition.end())
			{
				_need_check_condition.erase(odit);
			}
		}
	}
}


void context::calculate_position(const code_t& code, direction_type dir_type, offset_type offset_type, uint32_t volume, double_t price)
{
	LOG_INFO("calculate_position ", code.get_id(), dir_type, offset_type, volume, price);
	position_info p;
	auto it = _position_info.find(code);
	if (it != _position_info.end())
	{
		p = it->second;
	}
	else
	{
		p.id = code;
	}
	if (offset_type == offset_type::OT_OPEN)
	{
		if (dir_type == direction_type::DT_LONG)
		{
			p.today_long.postion += volume;
			p.long_pending -= volume;

		}
		else
		{
			p.today_short.postion += volume;
			p.short_pending -= volume;
		}
	}
	else if (offset_type == offset_type::OT_CLSTD)
	{
		if (dir_type == direction_type::DT_LONG)
		{
			p.today_long.postion -= volume;
			p.today_long.frozen -= volume;
		}
		else if (dir_type == direction_type::DT_SHORT)
		{
			p.today_short.postion -= volume;
			p.today_short.frozen -= volume;
		}
	}
	else
	{
		if (dir_type == direction_type::DT_LONG)
		{
			p.history_long.postion -= volume;
			p.history_long.frozen -= volume;
		}
		else if (dir_type == direction_type::DT_SHORT)
		{
			p.history_short.postion -= volume;
			p.history_short.frozen -= volume;
		}
	}
	if (!p.empty())
	{
		_position_info[code] = p;
	}
	else
	{
		if (it != _position_info.end())
		{
			_position_info.erase(it);
		}
	}
	print_position("calculate_position");
}

void context::frozen_deduction(const code_t& code, direction_type dir_type, offset_type offset_type, uint32_t volume)
{
	auto it = _position_info.find(code);
	if (it == _position_info.end())
	{
		return;
	}
	position_info& pos = it->second;
	if (offset_type == offset_type::OT_CLSTD)
	{
		if (dir_type == direction_type::DT_LONG)
		{
			pos.today_long.frozen += volume;
		}
		else if (dir_type == direction_type::DT_SHORT)
		{
			pos.today_short.frozen += volume;
		}
	}
	else if (offset_type == offset_type::OT_CLOSE)
	{
		if (dir_type == direction_type::DT_LONG)
		{
			pos.history_long.frozen += volume;
		}
		else if (dir_type == direction_type::DT_SHORT)
		{
			pos.history_short.frozen += volume;
		}
	}
	print_position("frozen_deduction");
}
void context::unfreeze_deduction(const code_t& code, direction_type dir_type, offset_type offset_type, uint32_t volume)
{
	auto it = _position_info.find(code);
	if (it == _position_info.end())
	{
		return;
	}
	position_info& pos = it->second;
	if (offset_type == offset_type::OT_CLSTD)
	{
		if (dir_type == direction_type::DT_LONG)
		{
			if (pos.today_long.frozen > volume)
			{
				pos.today_long.frozen -= volume;
			}
			else
			{
				pos.today_long.frozen = 0;
			}
		}
		else if (dir_type == direction_type::DT_SHORT)
		{
			if (pos.today_short.frozen > volume)
			{
				pos.today_short.frozen -= volume;
			}
			else
			{
				pos.today_short.frozen = 0;
			}
		}
	}
	else if (offset_type == offset_type::OT_CLOSE)
	{
		if (dir_type == direction_type::DT_LONG)
		{
			if (pos.history_long.frozen > volume)
			{
				pos.history_long.frozen -= volume;
			}
			else
			{
				pos.history_long.frozen = 0;
			}
		}
		else if (dir_type == direction_type::DT_SHORT)
		{
			if (pos.history_short.frozen > volume)
			{
				pos.history_short.frozen -= volume;
			}
			else
			{
				pos.history_short.frozen = 0;
			}
		}
	}
	print_position("thawing_deduction");
}

void context::record_pending(const code_t& code, direction_type dir_type, offset_type offset_type, uint32_t volume)
{
	print_position("record_pending begin");
	if(offset_type== offset_type::OT_OPEN)
	{
		auto& pos = _position_info[code];
		pos.id = code ;
		if(dir_type == direction_type::DT_LONG)
		{
			pos.long_pending += volume;
		}
		else if (dir_type == direction_type::DT_SHORT)
		{
			pos.short_pending += volume;
		}
	}
	print_position("record_pending end");
}

void context::recover_pending(const code_t& code, direction_type dir_type, offset_type offset_type, uint32_t volume)
{
	print_position("recover_pending begin");
	if (offset_type == offset_type::OT_OPEN)
	{
		auto it = _position_info.find(code);
		if(it != _position_info.end())
		{
			if (dir_type == direction_type::DT_LONG)
			{
				it->second.long_pending -= volume;
			}
			else if (dir_type == direction_type::DT_SHORT)
			{
				it->second.short_pending -= volume;
			}
		}
	}
	print_position("recover_pending end");
}


void context::set_trading_filter(filter_function callback)
{
	_filter_function = callback;
}

void context::check_condition()
{

	for (auto it = _need_check_condition.begin(); it != _need_check_condition.end();)
	{
		if (it->second(it->first))
		{
			if (this->get_order(it->first).invalid() || this->cancel_order(it->first))
			{
				it = _need_check_condition.erase(it);
			}
			else
			{
				++it;
			}
		}
		else
		{
			++it;
		}
	}
}

void context::remove_condition(estid_t estid)
{
	auto odit = _need_check_condition.find(estid);
	if (odit != _need_check_condition.end())
	{
		_need_check_condition.erase(odit);
	}
}

void context::clear_condition()
{
	_need_check_condition.clear();
}

double_t context::get_price_step(const code_t& code)const
{
	if (_ps_config)
	{
		return _ps_config->get_price_step(code);
	}
	LOG_WARNING("_price_step_config null");
	return .0;
}

void context::regist_order_listener(estid_t estid, order_listener* listener)
{
	_order_listener[estid] = listener;
}