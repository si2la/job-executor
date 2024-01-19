DROP TABLE IF EXISTS WebCCSettings;
CREATE TABLE WebCCSettings(id INTEGER PRIMARY KEY AUTOINCREMENT, ccid INT, uservarId INT, onVal TEXT, offVal TEXT, max INT, min INT, step INT, suffix TEXT, type TEXT, location TEXT);
INSERT INTO WebCCSettings VALUES(1, 1, null, '1', '0', 1, 0, 1, '', 'checkbox', 'page_table'); -- для элемента управления Лампа кухня пользуемся checkbox'ом

DROP TABLE IF EXISTS UserVariables;
CREATE TABLE UserVariables(id INTEGER PRIMARY KEY AUTOINCREMENT, varname TEXT, alias TEXT, urls TEXT, controlType TEXT);
INSERT INTO UserVariables VALUES(1, 'user_first', 'Переменная 1', '/wpage/1#', 'text');

DROP TABLE IF EXISTS WebCCTypes;
CREATE TABLE WebCCTypes(id INTEGER PRIMARY KEY AUTOINCREMENT, value TEXT);
INSERT INTO WebCCTypes VALUES(1, 'checkbox');
INSERT INTO WebCCTypes VALUES(2, 'number');
INSERT INTO WebCCTypes VALUES(3, 'range');
INSERT INTO WebCCTypes VALUES(4, 'text');

DROP TABLE IF EXISTS WebVarTypes;
CREATE TABLE WebVarTypes(id INTEGER PRIMARY KEY AUTOINCREMENT, value TEXT);
INSERT INTO WebVarTypes VALUES(1, 'text');
INSERT INTO WebVarTypes VALUES(2, 'color');

