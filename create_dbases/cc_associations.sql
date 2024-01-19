DROP TABLE IF EXISTS Ccassociations;
CREATE TABLE Ccassociations(id INTEGER PRIMARY KEY AUTOINCREMENT, ccidSens INT, ccidAct INT);
-- SELECT Ccassociations.ccidSens AS sensorid, Connchannels.alias AS sensalias FROM Ccassociations INNER JOIN Connchannels ON sensorid = Connchannels.id;

-- SELECT Ccassociations.ccidAct AS actuatorid, Connchannels.alias AS actalias FROM Ccassociations INNER JOIN Connchannels ON actuatorid = Connchannels.id;
