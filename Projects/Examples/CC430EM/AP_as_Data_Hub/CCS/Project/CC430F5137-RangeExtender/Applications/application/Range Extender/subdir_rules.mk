################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
Applications/application/Range\ Extender/main_AP_RE.obj: C:/Texas\ Instruments/SimpliciTI-CCS-1.1.1/Projects/Examples/Applications/main_AP_RE.c $(GEN_OPTS) $(GEN_SRCS)
	@echo 'Building file: $<'
	@echo 'Invoking: MSP430 Compiler'
	"C:/ti/ccsv5/tools/compiler/msp430_4.1.1/bin/cl430" --cmd_file="C:/Texas Instruments/SimpliciTI-CCS-1.1.1/Projects/Examples/CC430EM/AP_as_Data_Hub/CCS/Project/../Configuration/smpl_nwk_config.dat" --cmd_file="C:/Texas Instruments/SimpliciTI-CCS-1.1.1/Projects/Examples/CC430EM/AP_as_Data_Hub/CCS/Project/../Configuration/Range_Extender/smpl_config.dat"  --silicon_version=mspx -g --define=__CC430F5137__ --define=MRFI_CC430 --include_path="C:/ti/ccsv5/ccs_base/msp430/include" --include_path="C:/ti/ccsv5/msp430/include" --include_path="C:/ti/ccsv5/tools/compiler/msp430_4.1.1/include" --include_path="C:/ti/ccsv5/tools/compiler/msp430_4.1.1/include" --include_path="C:/Texas Instruments/SimpliciTI-CCS-1.1.1/Components/bsp" --include_path="C:/Texas Instruments/SimpliciTI-CCS-1.1.1/Components/bsp/boards/CC430EM" --include_path="C:/Texas Instruments/SimpliciTI-CCS-1.1.1/Components/bsp/drivers" --include_path="C:/Texas Instruments/SimpliciTI-CCS-1.1.1/Components/mrfi" --include_path="C:/Texas Instruments/SimpliciTI-CCS-1.1.1/Components/simpliciti/nwk" --include_path="C:/Texas Instruments/SimpliciTI-CCS-1.1.1/Components/simpliciti/nwk_applications" --diag_warning=225 --printf_support=minimal --preproc_with_compile --preproc_dependency="Applications/application/Range Extender/main_AP_RE.pp" --obj_directory="Applications/application/Range Extender" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '


