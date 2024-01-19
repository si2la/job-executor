DROP TABLE IF EXISTS Users;
CREATE TABLE Users(id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT, passwd TEXT, displayName TEXT, email TEXT);
INSERT INTO Users VALUES(1, 'admin', 'admin', 'admin', 'jack@example.com'); -- администратор контроллера
INSERT INTO Users VALUES(2, 'leo', 'leo', 'Leo', 'leo@razumdom.ru'); -- пользователь без административных прав

DROP TABLE IF EXISTS Webpages;
CREATE TABLE Webpages(id INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT, fullName TEXT, imageUrl TEXT, access INT);
INSERT INTO Webpages VALUES(1, '', 'Первый этаж', '/pictures/custom/plan.jpg', '1'); -- first user page




