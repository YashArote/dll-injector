#include "hook_utils.h"
#include "flutter_windows.h"
#include <flutter/method_channel.h>
#include <iostream>
#include<Windows.h>
#include <flutter/standard_method_codec.h>

void sayHello(){
    std::cout<<"Hello World";
}
