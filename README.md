# touch-timeout
A daemon that manages brightness and dims an official RPi 7" display. Meant to be used on a minimal linux distribution like HifiBerry OS.
| Event                           | Action                                                   |
| ------------------------------- | -------------------------------------------------------- |
| Boot                            | Reads `/etc/touch-timeout.conf`, applies settings        |
| Touchscreen idle (½ timeout)    | Dims to 10 or `user_brightness/10`, whichever is greater |
| Touchscreen idle (full timeout) | Turns off (brightness = 0)                               |
| Touch detected                  | Restores to full brightness                              |
| Missing config                  | Uses defaults (100 brightness, 300s timeout)             |
| Systemd stop/restart            | Cleans up file descriptors safely                        |
| CPU Idle                        | < 0.2%                                                   |

To build and deploy:

```bash
gcc -O2 -Wall -o touch-timeout touch-timeout.c
sudo mv touch-timeout /usr/bin/
sudo chmod 755 /usr/bin/touch-timeout
sudo mv touch-timeout.service /etc/systemd/system/
sudo nano /etc/touch-timeout.conf     # edit your values
sudo systemctl daemon-reload
sudo systemctl enable touch-timeout.service
sudo systemctl start touch-timeout.service
```
