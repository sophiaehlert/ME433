import time
import board
import pwmio

# create PWM on GP16
pwm = pwmio.PWMOut(board.GP16, frequency=50)

def set_servo_angle(angle):
    # map 0–180 to duty cycle
    min_duty = 1638   # ~1ms
    max_duty = 8192   # ~2ms

    duty = int(min_duty + (angle / 180) * (max_duty - min_duty))
    pwm.duty_cycle = duty

while True:
    # sweep forward
    for angle in range(0, 181, 2):
        set_servo_angle(angle)
        time.sleep(0.02)

    # sweep backward
    for angle in range(180, -1, -2):
        set_servo_angle(angle)
        time.sleep(0.02)