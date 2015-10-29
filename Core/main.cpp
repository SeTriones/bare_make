//
////                       _oo0oo_
////                      o8888888o
////                      88" . "88
////                      (| -_- |)
////                      0\  =  /0
////                    ___/`---'\___
////                  .' \\|     |// '.
////                 / \\|||  :  |||// \
////                / _||||| -:- |||||- \
////               |   | \\\  -  /// |   |
////               | \_|  ''\---/''  |_/ |
////               \  .-\__  '-'  ___/-. /
////             ___'. .'  /--.--\  `. .'___
////          ."" '<  `.___\_<|>_/___.' >' "".
////         | | :  `- \`.;`\ _ /`;.`/ - ` : | |
////         \  \ `_.   \_ __\ /__ _/   .-` /  /
////     =====`-.____`.___ \_____/___.-`___.-'=====
////                       `=---='
////
////
////     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
////
////               ·ð×æ±£ÓÓ         ÓÀÎÞBUG
////
////
////
//
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <locale.h>
#include <malloc.h>
#include <Common/service_log.hpp>
#include <unistd.h>
#include "http_handler.hpp"

void sigterm_handler(int signo) {}
void sigint_handler(int signo) {}

int main(int argc, char* argv[]) {
	mallopt(M_MMAP_THRESHOLD, 256*1024);
	setenv("LC_CTYPE", "zh_CN.gbk", 1);
	close(STDIN_FILENO);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, &sigterm_handler);
	signal(SIGINT, &sigint_handler);

    http_handler* handler = new http_handler(4, 20);
    handler->open(10086, 10, (1 << 10) * 256);
    _INFO("ready to activate");
    handler->activate();
    _INFO("start!");
    pause();
    _INFO("stop");
	handler->stop();
    delete handler;

    return 0;
}
