#ifndef WARSHIPS_FIELD_H
#define WARSHIPS_FIELD_H

#define PLAYER_ONE 0
#define PLAYER_TWO 1

#define FIELD_WIDTH 10
#define FIELD_HEIGHT 10

#define true 1
#define false 0

#define CELL_EMPTY      (0)
#define CELL_SHOT      (1)
#define CELL_SHIP      (2)
#define CELL_DESTROYED_SHIP     (3)

#define CELL_EMPTY_CHAR ('.')
#define CELL_SHOT_CHAR ('*')
#define CELL_SHIP_CHAR ('@')
#define CELL_DESTROYED_SHIP_CHAR ('x')



typedef struct warships_cell {
    char value;
} warships_cell_t;

typedef struct warships_field {
    int fieldWidth;
    int fieldHeight;
    warships_cell_t cells[FIELD_HEIGHT][FIELD_WIDTH];
} warships_field_t;

typedef struct warships_gamedata
{
    warships_field_t first_player_field;
    warships_field_t second_player_field;
    int playerTurn;
}warships_gamedata_t;

char GetCellChar(short cellValue) ;
void PrintField(warships_field_t *field);
void PrintGameData(warships_gamedata_t *gameData, int playerIndex);
warships_field_t HideField(const warships_field_t gameField);

#define MSG_DATA_LENGTH 1024
typedef struct message_data{
    char type;
    char data[MSG_DATA_LENGTH];
}message_data_t;

#define MSG_TYPE_INFO 1 //then str
#define MSG_TYPE_COMMAND_JOIN 2 //then password as args
#define MSG_TYPE_COMMAND_START 3 //then password as args
#define MSG_TYPE_GAME_DATA 4 //then gameData
#define MSG_TYPE_WARSHIPS_TURN 5 //then int int as X Y



message_data_t BytesToMessageData(char* message);
//char * MessageDataToBytes();

#endif

