DROP TABLE IF EXISTS Sysports;
CREATE TABLE Sysports(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, osName TEXT, active INT);
INSERT INTO Sysports VALUES(1, 'UART1', '/dev/ttymxc4', 1);
INSERT INTO Sysports VALUES(2, 'UART2', '/dev/ttymxc1', 1);
INSERT INTO Sysports VALUES(3, 'ETH0', 'eth0', 1);
INSERT INTO Sysports VALUES(4, 'ETH1', 'eth1', 1);
-- INSERT INTO Sysports VALUES(5, 'MQTT', 'localhost', 1); MQTT будет через eth0, eth1!!!

DROP TABLE IF EXISTS Conndevices;
CREATE TABLE Conndevices(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, profileId INT, alias TEXT, active INT);
INSERT INTO Conndevices VALUES(1, 'DRM88ER', 1, 'I этаж', 1);
INSERT INTO Conndevices VALUES(2, 'MQTT client', 100, 'Клиент локального сервиса MQTT', 1);
-- INSERT INTO Conndevices VALUES(3, 'MSU24', 7, 'Теплица', 1);

-- убрать поле profileioId!!!
-- поле portSpeed переименовать в ipaddrOrPortSpeed в случае MODBUS_TCP и MQTT здесь будет ip-адрес:порт устройства, а если протокол MODBUS_RTU - то тут скорость
-- paddress теперь будет содержать адрес протокола (модбас) в случае MODBUS_RTU и MODBUS_TCP, в случае MQTT здесь не будет ничего
-- 29.05.2020 последнее утверждение неверно! в случае MQTT здесь IP-адрес и порт брокера, поэтому же и название поля оставляем 
DROP TABLE IF EXISTS Cdprotos;
CREATE TABLE Cdprotos(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, conndeviceId INT, profileioId INT, portId INT, portSpeed TEXT, portParams TEXT, paddress TEXT, login TEXT, passwd TEXT);
INSERT INTO Cdprotos VALUES(1, 'MODBUS_RTU', 1, NULL, 1, '9600', '8-N-2', '34', '', '');
INSERT INTO Cdprotos VALUES(2, 'MODBUS_TCP', 1, NULL, 3, '192.168.1.111:502', '', '34', '', '');
INSERT INTO Cdprotos VALUES(3, 'MQTT', 2, NULL, 3, 'localhost', '', '', '', '');

DROP TABLE IF EXISTS Connchannels;
CREATE TABLE Connchannels(id INTEGER PRIMARY KEY AUTOINCREMENT, conndeviceId INT, cdprotoId INT, funct TEXT, io INT, channelAddr TEXT,  alias TEXT, active INT, urls TEXT);
INSERT INTO Connchannels VALUES(1, 1, 1, 'COILS', 1, '6', 'Лампа кухня', 1, '/wpage/1#');
INSERT INTO Connchannels VALUES(2, 1, 1, 'COILS', 0, '6', 'лампа кухня датчик', 1, '');
INSERT INTO Connchannels VALUES(3, 1, 1, 'COILS', 1, '4', 'Теплый пол', 1, '');
INSERT INTO Connchannels VALUES(4, 2, 3, 'PUB', 1, 'bathroom', 'Розетка с нагревателем в ванной', 1, '');

DROP TABLE IF EXISTS Ccassociations;
CREATE TABLE Ccassociations(id INTEGER PRIMARY KEY AUTOINCREMENT, ccidSens INT, ccidAct INT);
-- for example: INSERT INTO Ccassociations VALUES(1, 11, 9);

-- большой запрос для выбора параметров подключенного канала (по Connchannels.Id)
-- SELECT Connchannels.funct AS function, io, Connchannels.channelAddr AS channel_addr, Cdprotos.portSpeed AS port_speed,
-- Cdprotos.paddress AS device_addr, Sysports.osName AS port_os_name, Connchannels.alias AS actuator_alias, Conndevices.alias AS device_alias 
-- FROM Connchannels INNER JOIN Conndevices ON Conndevices.id = Connchannels.conndeviceId INNER JOIN Cdprotos ON Cdprotos.id = Connchannels.cdprotoId  
-- INNER JOIN Sysports ON Sysports.Id = Cdprotos.portId WHERE Connchannels.Id = 1;

-- 20.01.2020 изменение структуры таблицы 
-- ALTER TABLE Cdprotos ADD portParams TEXT;
-- !!!!!! где TEXT, там не должно быть NULL
