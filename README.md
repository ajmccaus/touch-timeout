# touch-timeout
A daemon that manages brightness and dims an official RPi 7" display

To build and deploy:
gcc -O2 -Wall -o touch-timeout touch-timeout.c
sudo mv touch-timeout /usr/bin/
sudo chmod 755 /usr/bin/touch-timeout
sudo mv touch-timeout.service /etc/systemd/system/
sudo nano /etc/touch-timeout.conf     # edit your values
sudo systemctl daemon-reload
sudo systemctl enable touch-timeout.service
sudo systemctl start touch-timeout.service
