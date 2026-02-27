
# Kitchen timer: 7-segment display with sound signaling timer on Arduino Nano 

This is the project for a simple Arduino-based timer consisting of:
- Arduino Nano board
- double 7-segment display
- buzzer
- control knob
- push button
- battery power supply

This device allows to set a time of up to 60 minutes after which the sound signal goes off. The time to be set is controlled using a knob mounted on a potentiometer. To start the countdown one has to press a button for ~100 ms. After that the display is locked and can't be changed by turning the knob. In the countdown mode integrated LED is toggled after every second and the displayed value is updated after every minute. When the timer reaches 0, the buzzer is activated and emits a sound signal. The displayed value changes to the current value corresponding to the knob position. To exit the countdown mode before reaching 0 the push button has to be pressed for ~3 s. Return to the default state is signaled by the buzzer. 

# Tips and tricks
- Due to a limited number of pins available with Arduino Nano board, connecting a 2-digit 7-segment display became a challenge. To overcome this problem, a human eye imperfection was taken advantage of. The corresponding segments from both parts are connected to the same microcontroller output, but when one digit is enabled the other one is disabled so they are never both on at the same time. The switching is done fast enough so that a human eye cannot see the difference between this workaround and a normal work.
- To prevent unwanted switching when the readout of a knob position is not stable in time (goes up or down a little), the moving average filtering is used.