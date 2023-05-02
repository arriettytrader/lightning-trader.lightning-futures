﻿#pragma once
#include <type_traits>
#include <define.h>
#include <data_types.hpp>
#include <lightning.h>

typedef untid_t straid_t;

class strategy
{
public :
	class param
	{
		std::map<std::string, std::string> _param;
	
	public:
		param(const char* str);

		template <typename T>
		T get(const char key[]) const
		{
			return get<T>(std::string(key));
		}
	
	private:
		template <typename T>
		typename std::enable_if<std::is_same<T, const char *>::value, T>::type get(const std::string &key) const
		{
			auto it = _param.find(key);
			if (it == _param.end())
			{
				return "";
			}
			return it->second.c_str();
		}

		template <typename T>
		typename std::enable_if<std::is_same<T, int32_t>::value, T>::type get(const std::string &key) const
		{
			auto it = _param.find(key);
			if (it == _param.end())
			{
				return 0X0D;
			}
			return std::atoi(it->second.c_str());
		}

		template <typename T>
		typename std::enable_if<std::is_same<T, uint32_t>::value, T>::type get(const std::string &key) const
		{
			auto it = _param.find(key);
			if (it == _param.end())
			{
				return 0X0U;
			}
			return static_cast<uint32_t>(std::atoi(it->second.c_str()));
		}

		template <typename T>
		typename std::enable_if<std::is_same<T, double_t>::value, T>::type get(const std::string &key) const
		{
			auto it = _param.find(key);
			if (it == _param.end())
			{
				return .0F;
			}
			return std::atof(it->second.c_str());
		}
		template <typename T>
		typename std::enable_if<std::is_same<T, int64_t>::value, T>::type get(const std::string &key) const
		{
			auto it = _param.find(key);
			if (it == _param.end())
			{
				return 0X0LL;
			}
			return std::atoll(it->second.c_str());
		}
		template <typename T>
		typename std::enable_if<std::is_same<T, uint64_t>::value, T>::type get(const std::string &key) const
		{
			auto it = _param.find(key);
			if (it == _param.end())
			{
				return 0X0LLU;
			}
			return static_cast<uint64_t>(std::atoll(it->second.c_str()));
		}
	};
private:

	straid_t _id;

	class strategy_manager* _manager;

public:

	strategy();
	
	virtual ~strategy();

	/*
	 *	初始化
	 */
	void init(straid_t id,class strategy_manager* manager);

	// 回调函数
private:
	/*
	 *	初始化事件
	 *	生命周期中只会回调一次
	 */
	virtual void on_init(){};

public:
	/*
	 *	交易日初始化完成
	 */
	virtual void on_ready() {};

	/*
	 *	tick推送
	 */
	virtual void on_tick(const tick_info& tick, const deal_info& deal) {}

	/*
	 *	订单接收回报
	 *  @is_success	是否成功
	 *	@order	本地订单
	 */
	virtual void on_entrust(const order_info& order) {};

	/*
	 *	成交回报
	 *
	 *	@localid	本地订单id
	 */
	virtual void on_deal(estid_t localid,uint32_t deal_volume, uint32_t total_volume) {}

	/*
	 *	成交完成回报
	 *
	 *	@localid	本地订单id
	 */
	virtual void on_trade(estid_t localid, const code_t& code, offset_type offset, direction_type direction, double_t price, uint32_t volume) {}

	/*
	 *	撤单
	 *	@localid	本地订单id
	 */
	virtual void on_cancel(estid_t localid, const code_t& code, offset_type offset, direction_type direction, double_t price, uint32_t cancel_volume, uint32_t total_volume) {}

	/*
	 *	错误
	 *	@localid	本地订单id
	 *	@error 错误代码
	 */
	virtual void on_error(error_type type, estid_t localid, const uint32_t error) {}

	/*
	 *	销毁
	 */
	virtual void on_destory() {}

protected:
	// 功能函数
	/*
	 *	开多单
	 *	code 期货代码 SHFF.rb2301
	 *  price 如果是0表示市价单，其他表示现价单
	 *  flag 默认为正常单
	 *	@localid	本地订单id
	 */
	estid_t buy_for_open(const code_t& code, uint32_t count, double_t price = 0, order_flag flag = OF_NOR);

	/*
	 *	平多单
	 *	code 期货代码 SHFF.rb2301
	 *  price 如果是0表示市价单，其他表示现价单
	 *  flag 默认为正常单
	 *	@localid	本地订单id
	 */
	estid_t sell_for_close(const code_t& code, uint32_t count, double_t price = 0, order_flag flag = OF_NOR);

	/*
	 *	开空单
	 *	code 期货代码 SHFF.rb2301
	 *  price 如果是0表示市价单，其他表示现价单
	 *  flag 默认为正常单
	 *	@localid	本地订单id
	 */
	estid_t sell_for_open(const code_t& code, uint32_t count, double_t price = 0, order_flag flag = OF_NOR);

	/*
	 *	平空单
	 *	code 期货代码 SHFF.rb2301
	 *  price 如果是0表示市价单，其他表示现价单
	 *  flag 默认为正常单
	 *	@localid	本地订单id
	 */
	estid_t buy_for_close(const code_t& code, uint32_t count, double_t price = 0, order_flag flag = OF_NOR);

	/*
	 *	下单单
	 *	order_id 下单返回的id
	 */
	estid_t place_order(offset_type offset, direction_type direction, const code_t& code, uint32_t count, double_t price = 0, order_flag flag = OF_NOR);
	/*
	 *	撤单
	 *	order_id 下单返回的id
	 */
	void cancel_order(estid_t order_id);

	/**
	 * 获取仓位信息
	 */
	const position_info& get_position(const code_t& code) const;

	/**
	 * 获取账户资金
	 */
	const account_info& get_account() const;

	/**
	 * 获取委托订单
	 **/
	const order_info& get_order(estid_t order_id) const;

	/**
	 * 订阅行情
	 **/
	void subscribe(const code_t& code) ;

	/**
	 * 取消订阅行情
	 **/
	void unsubscribe(const code_t& code) ;

	/**
	 * 获取时间
	 *
	 */
	time_t get_last_time() const ;

	/**
	* 使用自定义交易通道
	*/
	void use_custom_chain(bool flag);

	/*
	 * 设置撤销条件(返回true时候撤销)
	 */
	void set_cancel_condition(estid_t order_id, std::function<bool(const tick_info&)> callback);

	/**
	 * 获取最后一次下单时间
	 *	跨交易日返回0
	 */
	time_t last_order_time();



	inline straid_t get_id()
	{
		return _id;
	}

	/**
	 * 获取用户数据，直接写入会被保存到共享内存中
	 *	注意多个策略时候id不能改变
	 *	ID最大值与localdb配置中的userdata_block对应
	 */

	void* get_userdata(size_t size);

	/**
	 * 获取交易日
	 */
	uint32_t get_trading_day()const;

	/**
	 *	是否准备就绪
	 */
	bool is_trading_ready()const;

};


