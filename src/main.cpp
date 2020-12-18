#include "../include/logger.h"
#include <cstdio>
#include <string> // for strings

int main(int argc, char **argv) {
  homemadecam::logger::info("试试看！");
  homemadecam::logger::info("我再试试看！");
  return 0;
}