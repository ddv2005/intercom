################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include zeromq/src/subdir.mk
-include swig/nativesrc/subdir.mk
-include sqlite3/subdir.mk
-include serial/subdir.mk
-include pjsupport/src/subdir.mk
-include lua/src/subdir.mk
-include lua/luasocket/subdir.mk
-include lua/bitop/subdir.mk
-include lua/subdir.mk
-include libconfig/subdir.mk
-include crypto/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: intercom

# Tool invocations
intercom: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross G++ Linker'
	g++ -s -L/projects/intercom/3rdparty/pjproject/pjsip/lib -L/projects/intercom/3rdparty/pjproject/third_party/lib -L/projects/intercom/3rdparty/pjproject/pjnath/lib -L/projects/intercom/3rdparty/pjproject/pjmedia/lib -L/projects/intercom/3rdparty/pjproject/pjlib-util/lib -L/projects/intercom/3rdparty/pjproject/pjlib/lib -O3 -fwhole-program -Wl,--gc-sections -fno-exceptions -static-libstdc++  -fvisibility=hidden -fvisibility-inlines-hidden -fdata-sections -ffunction-sections -fomit-frame-pointer -ftree-vectorize -o "intercom" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(C++_DEPS)$(OBJS)$(C_DEPS)$(CC_DEPS)$(CPP_DEPS)$(EXECUTABLES)$(CXX_DEPS)$(C_UPPER_DEPS) intercom
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
