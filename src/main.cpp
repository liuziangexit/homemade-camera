#include "util/thread_pool.h"
#include <iostream>

int main() {
  hcam::thread_pool tp;
  tp.run(4);
  auto fut = tp.async([] {
    std::cout << "naive" << std::endl;
    return 999;
  });
  fut.wait();
  // getchar();
  tp.stop();
  std::cout << "Hello, World!" << std::endl;
  return 0;
}
