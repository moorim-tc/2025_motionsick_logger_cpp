After H/W installation, check these.

1. IMU 
 - Run Main code and check the log datasheet

2. RTC
 - i2cdetect -y 1 -> check address 'UU'
 - sudo hwclock -w
 - sudo hwclock -r

3. GPS
 - cat /dev/ttyAMA0

 