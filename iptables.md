sudo iptables -t nat -A  DOCKER -p tcp --dport 8081 -j DNAT --to-destination 172.17.0.2:8080

sudo iptables -t nat -A  DOCKER -p tcp --dport 8081 -j DNAT --to-destination 172.17.0.2:8080

sudo iptables -t nat --list-rules DOCKER
sudo iptables -t nat -A DOCKER ! -i docker0 -p tcp -m tcp --dport 8081 -j DNAT --to-destination 172.17.0.2:8080

sudo iptables -t nat --list-rules POSTROUTING
sudo iptables -t nat -A POSTROUTING -s 172.17.0.2/32 -d 172.17.0.2/32 -p tcp -m tcp --dport 8080 -j MASQUERADE

sudo iptables --list-rules DOCKER
sudo iptables -t filter -A DOCKER -d 172.17.0.2/32 ! -i docker0 -o docker0 -p tcp -m tcp --dport 8080 -j ACCEPT

sudo iptables -t nat -A  DOCKER -p tcp --dport 8081 -j DNAT --to-destination 192.168.0.200:8080 

curl 172.17.0.2:8080/test/hello

sudo iptables -t nat -vnL DOCKER --line-number

sudo iptables -t nat -D DOCKER 4