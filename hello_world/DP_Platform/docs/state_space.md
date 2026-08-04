# State_space
![State_space_image](images/System_space.png)

## SYSTEM_BOOT​
System startup state. 
During this phase, all necessary modules are initialized, hardware is configured, and the device is prepared for operation. 
Upon successful completion of the startup process, the system automatically transitions to the homing state (SYSTEM_HOMING).

## SYSTEM_HOMING​
The state responsible for the axis homing procedure. 
Its purpose is to determine the reference position of the drives so that subsequent movements can be executed with the required accuracy. 
If the homing process completes successfully, the system transitions to the ready state (SYSTEM_READY); 
in the event of an error, it transitions to the error state (SYSTEM_ERROR).

## SYSTEM_READY​
Operational readiness state. 
In this state, the system performs no movements and awaits a new control command. 
This is the primary operating state in which the device remains for the majority of the time. 
Upon receipt of a valid command, the system transitions to the task execution state (SYSTEM_RUNNING).

## SYSTEM_RUNNING​
Execution status of the current command. 
The system executes the received command, such as moving to a specified position, performing a relative movement, or controlling the SPM platform using roll, pitch, and yaw parameters. 
Upon successful completion of the task, the system returns to the ready state (SYSTEM_READY), whereas if an error occurs, it transitions to the error state (SYSTEM_ERROR).

## SYSTEM_ERROR​
Error handling state. 
It occurs when an anomaly is detected during system operation. 
Once the cause of the error is resolved and a reset is performed, the system returns to the (SYSTEM_HOMING) state, allowing the reference position of the drives to be re-established before operation resumes.

## SYSTEM_EMERGENCY_STOP​
Emergency stop (E-STOP) state. 
It is activated by the safety button and causes an immediate stop of all drives and the interruption of ongoing operations. 
This state has the highest priority in the system. 
Upon release of the emergency stop, the system transitions to the SYSTEM_HOMING state to re-establish the reference position of the drives and ensure a safe resumption of operation.