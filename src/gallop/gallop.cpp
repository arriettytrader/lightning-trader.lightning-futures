﻿#include "gallop.h"
#include <define.h>
#include "demo_strategy.h"

#include "runtime_engine.h"
#include "evaluate_engine.h"

#pragma comment (lib,"lightning.lib")

void start_runtime()
{
	//auto app = runtime_engine("./rt_simnow.ini");
	auto app = runtime_engine("./runtime.ini");
	auto hcc = std::make_shared<demo_strategy>();
	app.add_strategy(0,hcc);
	app.run();
	
}

void start_evaluate(const std::vector<uint32_t>& all_trading_day)
{
	auto app = evaluate_engine("./evaluate.ini");
	/*
	std::vector<uint32_t> trading_day = { 
		20220801,
		20220802,
		20220803
		};
	*/
	auto hcc = std::make_shared<demo_strategy>();
	app.back_test(hcc, all_trading_day);
}



int main()
{
	
	std::vector<uint32_t> all_trading_day = {
		20220901,
		20220902,
		20220905,
		20220906,
		20220907,
		20220908,
		20220909,
		20220913,
		20220914,
		20220915,
		20220916,
		20220919,
		20220920,
		20220921,
		20220922,
		20220923,
		20220926,
		20220927,
		20220928,
		20220928,
		20220930
	};
	/*
	std::vector<uint32_t> all_trading_day = {
		20220801,
		20220802,
		20220803,
		20220804,
		20220805,
		20220808,
		20220809,
		20220810,
		20220811,
		20220812,
		20220815,
		20220816,
		20220817,
		20220818,
		20220819,
		20220822,
		20220823,
		20220824,
		20220825,
		20220826

	};
	*/
	
//max money : 99915.800000 i:[0] j:[3] k:[4] x:[2] y:[0]
	start_evaluate(all_trading_day);
	//start_hft_1_optimize(all_trading_day);
	//start_demo_optimize(all_trading_day);
	//start_runtime();
	
	/*
	LOG_DEBUG("123_%d----%s",123,"a");
	LOG_INFO("123_%d----%s", 123, "a");
	LOG_ERROR("123_%d----%s", 123, "a");
	LOG_WARNING("123_%d----%s", 123, "a");
	LOG_FATAL("123_%d----%s", 123, "a");
	*/
	
	return 0;
}
