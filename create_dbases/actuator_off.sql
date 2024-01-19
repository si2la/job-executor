INSERT INTO Scenarios (id, name, priority, deleted) VALUES(1, 'Выключить тепло', 0, 0); -- наивысший приоритет

INSERT INTO Actions (id, scenarioId, label, nStr, actOnce, operationId) VALUES(1, 1, '', 1, 1, 12);    -- сценарий "выключить тепло", нет метки, номер строки 1, выполнить один раз (1), 12 - операция "присвоить значение" или "присвоение"

INSERT INTO Parameters (actionId, pnumber, pvalue, ptype, ptag, connchannelId, pvariableType, ptimerRepeater, lastExecTime) VALUES(1, 1, '', 'Actuator', NULL, 3, '', '', 0);    -- действие (Action) id=7, номер параметра 1, значение не исп., "исполн. элемент", тега нет, канал - Теплый пол (COILS 1), ост - нет

INSERT INTO Parameters (actionId, pnumber, pvalue, ptype, ptag, connchannelId, pvariableType, ptimerRepeater, lastExecTime) VALUES(1, 2, 'OFF', 'Constant', 0, NULL, '', '', 0);    -- действие id=7, номер параметра 2, имя "OFF"., "константа", тег - значение (0), канал - NULL, остального - нет
