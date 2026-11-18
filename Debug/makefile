################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

CG_TOOL_ROOT := D:/Program Files/ti/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS

GEN_OPTS__FLAG := @"syscfg/device.opt" 
GEN_CMDS__FLAG := -Wl,-l"syscfg/device_linker.cmd" 

ORDERED_OBJS += \
"./empty_mspm0g3507.o" \
"./syscfg/ti_msp_dl_config.o" \
"./startup_mspm0g350x_ticlang.o" \
"./Hardware/IMU.o" \
"./Hardware/KEY.o" \
"./Hardware/OLED.o" \
"./Hardware/Track8.o" \
"./Hardware/emm42.o" \
"./Hardware/vision.o" \
"./Software/Debug.o" \
"./Software/LQR.o" \
"./Software/bsp_timer.o" \
$(GEN_CMDS__FLAG) \
-Wl,-ldevice.cmd.genlibs \
-Wl,-llibc.a \

-include ../makefile.init

RM := DEL /F
RMDIR := RMDIR /S/Q

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir_vars.mk
-include Hardware/subdir_vars.mk
-include Software/subdir_vars.mk
-include subdir_rules.mk
-include Hardware/subdir_rules.mk
-include Software/subdir_rules.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C55_DEPS)),)
-include $(C55_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(S67_DEPS)),)
-include $(S67_DEPS)
endif
ifneq ($(strip $(S62_DEPS)),)
-include $(S62_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(OPT_DEPS)),)
-include $(OPT_DEPS)
endif
ifneq ($(strip $(C??_DEPS)),)
-include $(C??_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(S??_DEPS)),)
-include $(S??_DEPS)
endif
ifneq ($(strip $(C64_DEPS)),)
-include $(C64_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(S64_DEPS)),)
-include $(S64_DEPS)
endif
ifneq ($(strip $(CLA_DEPS)),)
-include $(CLA_DEPS)
endif
ifneq ($(strip $(S55_DEPS)),)
-include $(S55_DEPS)
endif
ifneq ($(strip $(SV7A_DEPS)),)
-include $(SV7A_DEPS)
endif
ifneq ($(strip $(C62_DEPS)),)
-include $(C62_DEPS)
endif
ifneq ($(strip $(C67_DEPS)),)
-include $(C67_DEPS)
endif
ifneq ($(strip $(K_DEPS)),)
-include $(K_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C43_DEPS)),)
-include $(C43_DEPS)
endif
ifneq ($(strip $(S43_DEPS)),)
-include $(S43_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(SA_DEPS)),)
-include $(SA_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
EXE_OUTPUTS += \
Balance_Ball.out 

EXE_OUTPUTS__QUOTED += \
"Balance_Ball.out" 

BIN_OUTPUTS += \
Balance_Ball.hex 

BIN_OUTPUTS__QUOTED += \
"Balance_Ball.hex" 


# All Target
all: $(OBJS) $(GEN_CMDS)
	@$(MAKE) --no-print-directory -Onone "Balance_Ball.out" secondary-outputs

# Tool invocations
Balance_Ball.out: $(OBJS) $(GEN_CMDS)
	@echo 'Arm Linker - building target: "$@"'
	"D:/Program Files/ti/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang.exe" @"syscfg/device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -gdwarf-3 -Wl,-m"Balance_Ball.map" -Wl,-i"C:/ti/mspm0_sdk_2_11_00_07/source" -Wl,-i"D:/CCWorkSpace/Balance_Ball/Debug/syscfg" -Wl,-i"D:/Program Files/ti/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/lib" -Wl,--diag_wrap=off -Wl,--display_error_number -Wl,--warn_sections -Wl,--xml_link_info="Balance_Ball_linkInfo.xml" -Wl,--rom_model -o "Balance_Ball.out" $(ORDERED_OBJS)
	@echo 'Finished building target: "$@"'
	@echo ' '

Balance_Ball.hex: $(EXE_OUTPUTS)
	@echo 'Arm Hex Utility - building secondary target: "$@"'
	"D:/Program Files/ti/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmhex.exe" --memwidth=8 --romwidth=8 --diag_wrap=off --intel -o "Balance_Ball.hex" $(EXE_OUTPUTS__QUOTED)
	@echo 'Finished building secondary target: "$@"'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(GEN_MISC_FILES__QUOTED)$(BIN_OUTPUTS__QUOTED)$(GEN_FILES__QUOTED)$(EXE_OUTPUTS__QUOTED)
	-$(RM) "empty_mspm0g3507.o" "syscfg\ti_msp_dl_config.o" "startup_mspm0g350x_ticlang.o" "Hardware\IMU.o" "Hardware\KEY.o" "Hardware\OLED.o" "Hardware\Track8.o" "Hardware\emm42.o" "Hardware\vision.o" "Software\Debug.o" "Software\LQR.o" "Software\bsp_timer.o" 
	-$(RM) "empty_mspm0g3507.d" "syscfg\ti_msp_dl_config.d" "startup_mspm0g350x_ticlang.d" "Hardware\IMU.d" "Hardware\KEY.d" "Hardware\OLED.d" "Hardware\Track8.d" "Hardware\emm42.d" "Hardware\vision.d" "Software\Debug.d" "Software\LQR.d" "Software\bsp_timer.d" 
	-$(RMDIR) $(GEN_MISC_DIRS__QUOTED)
	-@echo ' '

secondary-outputs: $(BIN_OUTPUTS)

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets

