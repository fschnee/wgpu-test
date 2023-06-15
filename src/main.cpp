#include <stdio.h>

#include <webgpu/webgpu.hpp>

int main()
{
    auto instance = wgpu::createInstance(wgpu::InstanceDescriptor{});
    
    printf("Howdy world\n");
    return 0;
}
