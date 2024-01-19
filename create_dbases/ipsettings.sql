DROP TABLE IF EXISTS Ipsettings;
CREATE TABLE Ipsettings (id INTEGER PRIMARY KEY AUTOINCREMENT, ipaddr TEXT, ipmask TEXT, gateway TEXT, dns1 TEXT, dns2 TEXT, dhcpst TEXT, iface TEXT);
INSERT INTO Ipsettings VALUES (NULL,'192.168.0.19','255.255.255.0','192.168.0.7','8.8.8.8','','static','eth0');
INSERT INTO Ipsettings VALUES (NULL,'192.168.1.111','255.255.255.0','192.168.1.1','8.8.8.8','','static','eth1');

