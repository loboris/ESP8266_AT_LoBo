/******************************************
 * BINTECHNOLOGY INTERRUPT VECTOR         *
 ******************************************/

.section .VectorBase.text
.type _VectorBase, %function
.literal_position
_VectorBase:
	.word _text_start

.section .DebugExceptionVector.text
.type _DebugExceptionVector, %function
.literal_position
_DebugExceptionVector:
	call0 bootloader_task
	nop

.section .NMIExceptionVector.text
.type _NMIExceptionVector, %function
.literal_position
_NMIExceptionVector:
	call0 bootloader_task
	nop

.section .KernelExceptionVector.text
.type _KernelExceptionVector, %function
.literal_position
_KernelExceptionVector:
	call0 bootloader_task
	nop

.section .UserExceptionVector.text
.type _UserExceptionVector, %function
.literal_position
_UserExceptionVector:
	call0 bootloader_task
	nop

.section .DoubleExceptionVector.text
.type _DoubleExceptionVector, %function
.literal_position
_DoubleExceptionVector:
	call0 bootloader_task
	nop
