#!/bin/bash

# Fetch Cloudflare IPs
cloudflare_ips=$(curl -s https://www.cloudflare.com/ips-v4)

# Loop through each IP and add UFW rules
while IFS= read -r ip; do
    echo "Adding rules for $ip"
    sudo ufw allow from "$ip" to any port 80 proto tcp
    sudo ufw allow from "$ip" to any port 443 proto tcp
done <<< "$cloudflare_ips"

sudo ufw deny 80/tcp
sudo ufw deny 443/tcp
