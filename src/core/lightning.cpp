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
#include "lightning.h"
#include "runtime.h"
#include "evaluate.h"
#include "context.h"

extern "C"
{

	ltobj lt_create_context(context_type ctx_type, const char* config_path)
	{
		ltobj lt;
		lt.obj_type = ctx_type;
		if(ctx_type == CT_RUNTIME)
		{
			auto obj = new runtime();
			if (!obj->init_from_file(config_path))
			{
				delete obj;
				obj = nullptr;
			}
			
			lt.obj_ptr = obj;
			return lt;
		}
		if(ctx_type == CT_EVALUATE)
		{
			auto obj = new evaluate();
			if (!obj->init_from_file(config_path))
			{
				delete obj;
				obj = nullptr;
			}
			lt.obj_ptr = obj;
			return lt;
		}
		lt.obj_ptr = nullptr;
		return lt;
	}

	void lt_destory_context(ltobj& lt)
	{
		if(lt.obj_ptr)
		{
			if(lt.obj_type == CT_RUNTIME)
			{
				runtime* obj = (runtime*)lt.obj_ptr;
				delete obj;
				lt.obj_ptr = nullptr;
			}
			else if (lt.obj_type == CT_EVALUATE)
			{
				evaluate* obj = (evaluate*)lt.obj_ptr;
				delete obj;
				lt.obj_ptr = nullptr;
			}
		}
	}
	LT_INTERFACE_IMPLEMENTATION(bool, false, runtime, login_account, (const ltobj& lt), ());

	LT_INTERFACE_IMPLEMENTATION(void, VOID_DEFAULT, runtime, logout_account, (const ltobj& lt), ());

	LT_INTERFACE_IMPLEMENTATION(bool, false, context, start_service, (const ltobj& lt),());
	
	LT_INTERFACE_IMPLEMENTATION(bool, false, context, stop_service, (const ltobj& lt), ());

	LT_INTERFACE_IMPLEMENTATION(estid_t, INVALID_ESTID, context, place_order, (const ltobj& lt, offset_type offset, direction_type direction, const code_t& code, uint32_t count, double_t price, order_flag flag), (offset, direction, code, count, price, flag));
	
	LT_INTERFACE_IMPLEMENTATION(bool, false, context, cancel_order, (const ltobj& lt, estid_t estid), (estid));
	
	LT_INTERFACE_IMPLEMENTATION(const position_info&, default_position, context, get_position, (const ltobj& lt, const code_t& code), (code));
	
	LT_INTERFACE_IMPLEMENTATION(const order_info&, default_order, context, get_order, (const ltobj& lt, estid_t estid), (estid));

	LT_INTERFACE_IMPLEMENTATION(void, VOID_DEFAULT, context, subscribe, (const ltobj& lt, const std::set<code_t>& tick_data, tick_callback tick_cb), (tick_data, tick_cb));

	LT_INTERFACE_IMPLEMENTATION(void, VOID_DEFAULT, context, unsubscribe, (const ltobj& lt, const std::set<code_t>& tick_data), (tick_data));

	LT_INTERFACE_IMPLEMENTATION(daytm_t, 0, context, get_last_time, (const ltobj& lt), ());

	LT_INTERFACE_IMPLEMENTATION(void, VOID_DEFAULT, context, bind_realtime_event, (const ltobj& lt, const order_event& od_evt,cycle_callback init_cb, cycle_callback update_cb, cycle_callback destroy_cb), (od_evt, init_cb, update_cb, destroy_cb));

	LT_INTERFACE_IMPLEMENTATION(void, VOID_DEFAULT, evaluate, playback_history, (const ltobj& lt), ());

	LT_INTERFACE_IMPLEMENTATION(void, VOID_DEFAULT, evaluate, simulate_crossday, (const ltobj& lt, uint32_t trading_day), (trading_day));

	LT_INTERFACE_IMPLEMENTATION(daytm_t, 0, context, last_order_time, (const ltobj& lt), ());

	LT_INTERFACE_IMPLEMENTATION(const order_statistic&, default_statistic, context, get_order_statistic, (const ltobj& lt,const code_t& code), (code));

	LT_INTERFACE_IMPLEMENTATION(uint32_t, 0U, context, get_trading_day, (const ltobj& lt), ());
	
	LT_INTERFACE_IMPLEMENTATION(daytm_t, 0, context, get_close_time, (const ltobj& lt), ());

	LT_INTERFACE_IMPLEMENTATION(const char *, nullptr, context, get_include_config, (const ltobj& lt,const char* key), (key));

	LT_INTERFACE_IMPLEMENTATION(const tick_info&, default_tick, context, get_previous_tick, (const ltobj& lt, const code_t& code), (code));

	LT_INTERFACE_IMPLEMENTATION(const today_market_info&, default_today_market, context, get_today_market_info, (const ltobj& lt, const code_t& code), (code));

	
}