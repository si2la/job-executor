# JOB EXECUTOR (JEXECUTOR)

## ru
Проект - реализация задумок на тему: выполнения задач автоматизации по определенному расписанию (сценариям).
Проектировалось как часть ПО для контроллера Умного дома. Однако, нужно сделать что-то универсальное для задач автоматики.
Пока поддержка протоколов: Modbus, MQTT.

### Главные задачи которые решает приложение:
- список сценариев берется из базы данных SQLite (см. структуру БД в каталоге create_dbases) и выполняется в цикле по кругу - назовем это главной петлей приложения (есть разделение сценариев по приоритету);
- имеется возможность однократно-исполняемых событий (не сценариев), которые внедряются в главную петлю как события с наивысшим приоритетом.

### Вопросы, требующие доработки:
- отладка работы по MQTT-протоколу;
- увеличение быстродействия;
- выполнение программы на любом оборудовании (в том числе на микрокомпьютерах).

### Установка:
- требуются следующие инструменты:
  - SQLite,
  - gcc, make,
  - redis.

Для сценариев используется БД SQLite, установить ее в вашу систему можно по-разному (это касается интерфейса CLI - sqlite3,  необходимого скорее для отладки), однако чтобы скомпилировать приложение, необходимо скачать "объединенный тарбол" - https://www.sqlite.org/download.html#amalgtarball и распаковать его в папку с проектом.

Далее, приложение обменивается информацией с "внешним миром" через in-memory DB Redis. Его тоже необходимо установить в систему вместе с библиотекой для разработки hiredis.

Для дистрибутива Ubuntu/Mint/Debian это делается командой:

```
sudo apt install -y redis libhiredis-dev

```
На встраиваемую систему, это можно сделать из исходников, например.

## en
The project is the implementation of some ideas on the topic: performing automation tasks according to a specific schedule (scenarios).
Designed as software for a Smart Home controller to make sheduled tasks. However, I would like to make something universal for automation tasks.
Protocol support for now: Modbus, MQTT.

### The main tasks that the application solves:
- the list of events is taken from the SQLite database (see the database structure in the create_dbases directory) and executed in a loop in a circle - let's call this the main application loop (there is a division by priority);
- there is the possibility of one-time events (not scripts) that are injected into the main loop as events with the highest priority.

### Issues requiring improvement:
- debugging work using the MQTT protocol;
- increasing speed;
- execution of the program on any equipment (including on microcomputers).

### Installation:
- the following tools are required:
   - SQLite,
   - gcc, make,
   - redis.

To build scenaries, the SQLite database is used; you can install it on your system in different ways, but to compile the application, you need to download the "amalgamation tarball" - https://www.sqlite.org/download.html#amalgtarball and unpack it into a folder with project.

Next, the application exchanges information with the "outside world" through in-memory DB Redis. It also needs to be installed on the system along with the hiredis development library.

For the Ubuntu/Mint distribution, this is done with the command:

```
sudo apt install -y redis libhiredis-dev

```
For an embedded system, this can be done from source code, for example.


