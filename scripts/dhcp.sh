# Doesn't work in WSL, something is already listening at 10.255.255.254
dnsmasq --interface=tap0 --bind-interfaces --dhcp-range=192.168.100.10,192.168.100.80,12h  --log-dhcp