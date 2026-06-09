
#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <fstream>
#include <random>
#include <ctime>

using namespace std;

const int EMPTY = 0;
const int HUMAN = -1;
const int AI = 1;

struct GameResult {
    int result;
    vector<pair<int, int>> aiMoves;///site potezi na AIot {miniboard,pos}
};

class LearningAI {
public:
    unordered_map<string, pair<int, int>> moveStats;/// moveStats[miniboard_pos]={successes,total}
    vector<GameResult> gameHistory;///gi cuva site raboti od GameResults
    ///double learningRate = 0.1;
    double explorationRate = 0.0;

public:
    ///Evaluira kolku poeni ima covekot/AIot
    static double evaluateState(const vector<vector<int>>& boards, const vector<int>& metaStatus) {///meatstatus e za sekoj board koj pobedil
        int metaResult = checkMetaBoard(metaStatus);
        ///koj pobedi
        if (metaResult == AI) return +100000;
        if (metaResult == HUMAN) return -100000;
        if (metaResult == 2) return 0;

        int score = 0;
        const int WIN_WEIGHT = 1000;

        const int CENTER_WEIGHT = 60;
        const int CORNER_WEIGHT = 40;
        const int EDGE_WEIGHT = 20;

        int positionWeights[9] = {
            CORNER_WEIGHT, EDGE_WEIGHT, CORNER_WEIGHT,
            EDGE_WEIGHT, CENTER_WEIGHT, EDGE_WEIGHT,
            CORNER_WEIGHT, EDGE_WEIGHT, CORNER_WEIGHT
        };
        ///dodava poeni za okupirano pole i za imanje 2 osvoeni polinja vo red,kolona ili dijagonala
        for (int b = 0; b < 9; ++b) {
            int status = metaStatus[b];
            if (status == AI) score += WIN_WEIGHT;
            else if (status == HUMAN) score -= WIN_WEIGHT;
            else {
                for (int i = 0; i < 9; ++i) {
                    if (boards[b][i] == AI) {
                        score += positionWeights[i];
                    } else if (boards[b][i] == HUMAN) {
                        score -= positionWeights[i];
                    }
                }

                static const int wins[8][3] = {
                    {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
                    {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
                    {0, 4, 8}, {2, 4, 6}
                };
                for (int i = 0; i < 8; ++i) {
                    int a = wins[i][0], c = wins[i][2], d = wins[i][1];
                    int v1 = boards[b][a], v2 = boards[b][c], v3 = boards[b][d];

                    if ((v1 == AI && v2 == AI && v3 == EMPTY) ||
                        (v1 == AI && v3 == AI && v2 == EMPTY) ||
                        (v2 == AI && v3 == AI && v1 == EMPTY)) {
                        score += 50;
                    }
                    if ((v1 == HUMAN && v2 == HUMAN && v3 == EMPTY) ||
                        (v1 == HUMAN && v3 == HUMAN && v2 == EMPTY) ||
                        (v2 == HUMAN && v3 == HUMAN && v1 == EMPTY)) {
                        score -= 50;
                    }
                }
            }
        }
        ///pecati score
        return score;
    }
    ///zabelezuva uspesnost na sekoj poteg
    void learnFromGame(const GameResult& result) {
        gameHistory.push_back(result);

        for (const auto& move : result.aiMoves) {
            string moveKey = to_string(move.first) + "_" + to_string(move.second);///board_pos

            if (moveStats.find(moveKey) == moveStats.end()) {
                moveStats[moveKey] = {0, 0};
            }
            ///dodavanje na {successes,total}
            moveStats[moveKey].second++;
            if (result.result > 0) {
                moveStats[moveKey].first++;
            }
            else if (result.result < 0 && moveStats[moveKey].first > 0) {
                moveStats[moveKey].first--;
            }
        }
    }
    ///sortiranje po {successes,total}
    double getMovePreference(int board, int cell) {
        string moveKey = to_string(board) + "_" + to_string(cell);
        if (moveStats.find(moveKey) != moveStats.end()) {
            auto& stats = moveStats[moveKey];
            if (stats.second > 0) {
                return (double)stats.first / stats.second;
            }
        }
        return 0.3;
    }
    ///zacuvuvanje na minatata igra
    void saveToFile(const string& filename) {
        ofstream file(filename);
        if (file.is_open()) {
            file << moveStats.size() << "\n";
            for (const auto& pair : moveStats) {
                file << pair.first << " " << pair.second.first << " " << pair.second.second << "\n";
            }

            file << "GAME_HISTORY " << gameHistory.size() << "\n";
            for (const auto& g : gameHistory) {
                file << g.result << " " << g.aiMoves.size();
                for (auto& move : g.aiMoves)
                    file << " " << move.first << " " << move.second;
                file << "\n";
            }

            cout << "Learning data saved to " << filename << "\n";
            cout << "Saved " << moveStats.size() << " move statistics and " << gameHistory.size() << " games.\n";
        } else {
            cout << "Warning: Could not save learning data to " << filename << "\n";
        }
        file.close();
    }
    ///prevzema od minatiot game/games
    void loadFromFile(const string& filename) {
        ifstream file(filename);
        if (file.is_open()) {
            int moveCount;
            file >> moveCount;
            for (int i = 0; i < moveCount; i++) {
                string moveKey;
                int successes, total;
                file >> moveKey >> successes >> total;
                moveStats[moveKey] = {successes, total};
            }
            string tag;
            int gameCount;
            file >> tag >> gameCount;
            if (tag == "GAME_HISTORY") {
                for (int i = 0; i < gameCount; ++i) {
                    GameResult gr;
                    int moveCount;
                    file >> gr.result >> moveCount;
                    for (int j = 0; j < moveCount; ++j) {
                        int first, second;
                        file >> first >> second;
                        gr.aiMoves.push_back({first, second});
                    }
                    gameHistory.push_back(gr);
                }
            }
            cout << "Loaded " << moveStats.size() << " move statistics and " << gameHistory.size() << " games from " << filename << "\n";
        } else {
            cout << "No previous learning data found. AI will start learning from scratch.\n";
            cout << "Learning data will be saved to " << filename << " after the first game.\n";
        }
        file.close();
    }
    ///proveri dali nekoj ima pobedeno/ne e gotovo/nereseno i celosno popolneto
    static int checkSmallBoard(const vector<int>& cells) {
        static const int wins[8][3] = {
            {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
            {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
            {0, 4, 8}, {2, 4, 6}
        };
        for (int i = 0; i < 8; ++i) {
            int a = wins[i][0], b = wins[i][1], c = wins[i][2];
            if (cells[a] != EMPTY &&
                cells[a] == cells[b] &&
                cells[b] == cells[c]) {
                return cells[a];///-1 ili 1
            }
        }
        for (int i = 0; i < 9; ++i) {
            if (cells[i] == EMPTY)
                return 0;
        }
        return 2;
    }
    ///istoto za golemata tabla
    static int checkMetaBoard(const vector<int>& metaStatus) {
        static const int wins[8][3] = {
            {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
            {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
            {0, 4, 8}, {2, 4, 6}
        };
        for (int i = 0; i < 8; ++i) {
            int a = wins[i][0], b = wins[i][1], c = wins[i][2];
            if (metaStatus[a] != 0 && metaStatus[a] != 2 &&
                metaStatus[a] == metaStatus[b] &&
                metaStatus[b] == metaStatus[c]) {
                return metaStatus[a];///-1 ili 1
            }
        }
        for (int i = 0; i < 9; ++i) {
            if (metaStatus[i] == 0)
                return 0;
        }
        return 2;
    }

    int minimaxWithLearning(
        vector<vector<int>>& boards,///koj board i koe pole koj go ima osvoeno
        vector<int>& metaStatus,///koj koja minitabla ja ima osvoeno
        int currentPlayer,
        int nextBoard,
        int depth,
        int alpha,
        int beta,
        pair<int, int>& bestMove,///najdobar poteg
        vector<pair<int, int>>& currentGameMoves
    ) {
        int metaResult = checkMetaBoard(metaStatus);
        if (metaResult == AI) return +100000;
        if (metaResult == HUMAN) return -100000;
        if (metaResult == 2) return 0;
        ///ako depth==0 toa znaci deka zavrsuva rekurzijata
        if (depth == 0) {
            return evaluateState(boards, metaStatus);
        }
        ///generiranje na site potezi mozni
        auto moves = generateMoves(boards, metaStatus, nextBoard);
        if (moves.empty()) {
            return evaluateState(boards, metaStatus);
        }
        ///sortiranje spored {uspesnost,total}
        if (currentPlayer == AI) {
            sort(moves.begin(), moves.end(), [this](const pair<int, int>& a, const pair<int, int>& b) {
                return getMovePreference(a.first, a.second) > getMovePreference(b.first, b.second);
            });
        }

        if (currentPlayer == AI) {
            int maxEval = -1e9;///sakame da maksimizirame => setirame na najmala brojka
            pair<int, int> localBest = moves[0];///optimalen poteg
            for (auto& mv : moves) {
                ///za sekoj poteg kje zememe deka AIot go napravil i kje vidime sto kje se dobie ponatamu
                int b = mv.first, cell = mv.second;
                boards[b][cell] = AI;
                int oldStatus = metaStatus[b];
                updateSmallStatus(boards, metaStatus, b);
                int nextB = (metaStatus[cell] == 0) ? cell : -1;///sledna tabla

                currentGameMoves.push_back(mv);///potegot vo momentalnata simulacija na igra
                pair<int, int> tempMove(-1, -1);
                int eval = minimaxWithLearning(boards, metaStatus, HUMAN, nextB, depth - 1,
                    alpha, beta, tempMove, currentGameMoves);///odenje podlaboku vo rekurzijata

                ///brisenje na potegot i drugite raboti
                currentGameMoves.pop_back();

                boards[b][cell] = EMPTY;
                metaStatus[b] = oldStatus;

                if (eval > maxEval) {
                    maxEval = eval;
                    localBest = mv;
                }
                ///alpha e za maksimiziranje, a beta e za minimiziranje
                ///alpha e momentalno nivo, a beta e edno nivo nagore (parnode)
                ///setirame max na alpha do sega
                alpha = max(alpha, eval);
                /// ako alpha e pogolema vrednost od beta moze da prekineme so prebaruvanje vo taa granka
                /// bidejkji alpha moze da stane samo pogolemo (deka zimame max),
                /// ama beta vekje ima pomala opcija od porano
                /// pa nema nikogas da imame nova opcija na igranje vo taa granka
                if (beta <= alpha) break;
            }
            bestMove = localBest;
            return maxEval;
        }
        else {
                ///Igrame kako covekot => isto samo minimizirame
            int minEval = 1e9;
            pair<int, int> localBest = moves[0];
            for (auto& mv : moves) {
                int b = mv.first, cell = mv.second;
                boards[b][cell] = HUMAN;
                int oldStatus = metaStatus[b];
                updateSmallStatus(boards, metaStatus, b);
                int nextB = (metaStatus[cell] == 0) ? cell : -1;

                pair<int, int> tempMove(-1, -1);
                int eval = minimaxWithLearning(boards, metaStatus, AI, nextB, depth - 1,
                    alpha, beta, tempMove, currentGameMoves);

                boards[b][cell] = EMPTY;
                metaStatus[b] = oldStatus;

                if (eval < minEval) {
                    minEval = eval;
                    localBest = mv;
                }
                beta = min(beta, eval);
                if (beta <= alpha) break;
            }
            bestMove = localBest;
            return minEval;
        }
    }
    ///generiranje na site moves
    static vector<pair<int, int>> generateMoves(
        const vector<vector<int>>& boards,
        const vector<int>& metaStatus,
        int nextBoard
    ) {
        vector<pair<int, int>> moves;
        if (nextBoard != -1 && metaStatus[nextBoard] == 0) {
            for (int cell = 0; cell < 9; ++cell) {
                if (boards[nextBoard][cell] == EMPTY) {
                    moves.emplace_back(nextBoard, cell);
                }
            }
        }
        else {
            for (int b = 0; b < 9; ++b) {
                if (metaStatus[b] != 0) continue;
                for (int cell = 0; cell < 9; ++cell) {
                    if (boards[b][cell] == EMPTY) {
                        moves.emplace_back(b, cell);
                    }
                }
            }
        }
        return moves;
    }
    ///setira status na metaBoard
    static void updateSmallStatus(vector<vector<int>>& boards, vector<int>& metaStatus, int b) {
        metaStatus[b] = checkSmallBoard(boards[b]);
    }

    pair<int, int> exploreMove(const vector<pair<int, int>>& moves) {
        if (moves.empty()) return { -1, -1 };
        int index = rand() % moves.size();
        return moves[index];
    }
};

void printSmallBoard(const vector<int>& board) {
    for (int i = 0; i < 9; ++i) {
        char c = (board[i] == AI) ? 'X'
            : (board[i] == HUMAN) ? 'O'
            : '.';
        cout << c;
        if (i % 3 == 2 && i != 8) cout << "\n";
        else if (i % 3 != 2)      cout << " ";
    }
}

void printUltimate(const vector<vector<int>>& boards) {
    for (int blockRow = 0; blockRow < 3; ++blockRow) {
        for (int row = 0; row < 3; ++row) {
            for (int blockCol = 0; blockCol < 3; ++blockCol) {
                int b = blockRow * 3 + blockCol;
                for (int col = 0; col < 3; ++col) {
                    int idx = row * 3 + col;
                    char c = (boards[b][idx] == AI) ? 'X'
                        : (boards[b][idx] == HUMAN) ? 'O'
                        : '.';
                    cout << c;
                    if (col != 2) cout << " ";
                }
                if (blockCol != 2) cout << " | ";
            }
            cout << "\n";
        }
        if (blockRow != 2) cout << "---------------------\n";
    }
}

void playGameWithLearning() {
    LearningAI ai;
    cout << "Learning file will be saved in current directory.\n";
    cout << "Look for 'ai_learning.dat' in the same folder as your executable.\n\n";

    ai.loadFromFile("ai_learning.dat");

    vector<vector<int>> boards(9, vector<int>(9, EMPTY));
    vector<int> metaStatus(9, 0);
    int nextBoard = -1;
    int currentPlayer = HUMAN;

    GameResult currentGame;
    vector<pair<int, int>> gameMovesForLearning;

    cout << "Ultimate Tic-Tac-Toe with Learning AI!\n";
    cout << "You are 'O'. AI is 'X'.\n\n";

    while (true) {
        printUltimate(boards);
        cout << "\n";

        int metaResult = LearningAI::checkMetaBoard(metaStatus);
        if (metaResult == AI) {
            cout << "AI wins the game!\n";
            currentGame.result = 1;
            break;
        }
        else if (metaResult == HUMAN) {
            cout << "You win the game! Congratulations!\n";
            currentGame.result = -1;
            break;
        }
        else if (metaResult == 2) {
            cout << "The game is a draw!\n";
            currentGame.result = 0;
            break;
        }

        if (currentPlayer == HUMAN) {
            cout << "Your move.\n";
            int b, cell;

            if (nextBoard != -1 && metaStatus[nextBoard] == 0) {
                cout << "You must play in small board " << nextBoard << ".\n";
                b = nextBoard;

                cout << "Board " << b << " looks like:\n";
                printSmallBoard(boards[b]);
                cout << "\n";

                while (true) {
                    cout << "Enter cell index in board " << b << " (0-8): ";
                    cin >> cell;
                    if (cell < 0 || cell > 8) {
                        cout << "Invalid cell index. Try again.\n";
                        continue;
                    }
                    if (boards[b][cell] != EMPTY) {
                        cout << "That cell is occupied. Try another.\n";
                        continue;
                    }
                    break;
                }
            }
            else {
                cout << "You can play in any available small board.\n";
                cout << "Available boards: ";
                for (int i = 0; i < 9; i++) {
                    if (metaStatus[i] == 0) cout << i << " ";
                }
                cout << "\n";

                while (true) {
                    cout << "Enter small board index (0-8): ";
                    cin >> b;
                    if (b < 0 || b > 8) {
                        cout << "Invalid board index. Try again.\n";
                        continue;
                    }
                    if (metaStatus[b] != 0) {
                        cout << "That small board is finished. Try another.\n";
                        continue;
                    }
                    break;
                }

                cout << "Board " << b << " looks like:\n";
                printSmallBoard(boards[b]);
                cout << "\n";

                while (true) {
                    cout << "Enter cell index in board " << b << " (0-8): ";
                    cin >> cell;
                    if (cell < 0 || cell > 8) {
                        cout << "Invalid cell index. Try again.\n";
                        continue;
                    }
                    if (boards[b][cell] != EMPTY) {
                        cout << "That cell is occupied. Try another.\n";
                        continue;
                    }
                    break;
                }
            }

            boards[b][cell] = HUMAN;
            LearningAI::updateSmallStatus(boards, metaStatus, b);

            if (metaStatus[cell] == 0) {
                nextBoard = cell;
            }
            else {
                nextBoard = -1;
            }

            currentPlayer = AI;
        }
        else {
            cout << "AI is thinking...\n";
            pair<int, int> aiMove(-1, -1);
            vector<pair<int, int>> currentGameMoves;

            auto moves = LearningAI::generateMoves(boards, metaStatus, nextBoard);
            if (moves.empty()) {
                cout << "No moves available for AI!\n";
                break;
            }

            if ((rand() % 100) < (ai.explorationRate * 100)) {
                aiMove = ai.exploreMove(moves);
                cout << "AI is exploring a new move...\n";
            } else if (nextBoard == -1) {///ako moze da igra vo site miniboards, gi testira site
                int bestEval = numeric_limits<int>::min();
                pair<int, int> bestShallowMove = moves[0];
                for (auto& mv : moves) {
                    int b = mv.first, cell = mv.second;
                    boards[b][cell] = AI;
                    int oldStatus = metaStatus[b];
                    LearningAI::updateSmallStatus(boards, metaStatus, b);
                    int nextB = (metaStatus[cell] == 0) ? cell : -1;
                    pair<int, int> tmp(-1, -1);
                    vector<pair<int, int>> shallowMoves;
                    int eval = ai.minimaxWithLearning(boards, metaStatus, HUMAN, nextB, 2,///staveno e samo depth 2 deka inaku kje go nadmine TL
                        numeric_limits<int>::min(), numeric_limits<int>::max(),
                        tmp, shallowMoves);

                    int metaResult = LearningAI::checkMetaBoard(metaStatus);
                    if (metaResult == AI) {
                        aiMove = mv;
                        boards[b][cell] = EMPTY;
                        metaStatus[b] = oldStatus;
                        cout << "AI sees instant meta win and plays: board " << b << ", cell " << cell << "\n";
                        goto foundMetaWin;
                    }
                    ///setira max eval
                    if (eval > bestEval) {
                        bestEval = eval;
                        bestShallowMove = mv;
                    }
                    ///resetira
                    boards[b][cell] = EMPTY;
                    metaStatus[b] = oldStatus;
                }
                aiMove = bestShallowMove;
                foundMetaWin:;
            } else {
                ai.minimaxWithLearning(boards, metaStatus, AI, nextBoard, 7,///
                    numeric_limits<int>::min(), numeric_limits<int>::max(),
                    aiMove, currentGameMoves);/// deka zima & za aimove kje se zabelezi bestmove
            }

            int b = aiMove.first, cell = aiMove.second;
            cout << "AI plays in board " << b << ", cell " << cell << ".\n";
            gameMovesForLearning.push_back(aiMove);

            boards[b][cell] = AI;
            LearningAI::updateSmallStatus(boards, metaStatus, b);

            if (metaStatus[cell] == 0) {
                nextBoard = cell;
                cout << "Your next move must be in board " << nextBoard << ".\n";
            }
            else {
                nextBoard = -1;
                cout << "Board " << cell << " is already complete, so you can choose any available board.\n";
            }

            currentPlayer = HUMAN;
        }
        cout << "\n";
    }

    ///Kraj na igra

    currentGame.aiMoves = gameMovesForLearning;
    cout << "AI is learning from this game...\n";
    ai.learnFromGame(currentGame);
    ai.saveToFile("ai_learning.dat");

    cout << "Game completed. AI has learned from " << gameMovesForLearning.size()
        << " moves in this game.\n";

    char playAgain;
    cout << "Would you like to play another game? (y/n): ";
    cin >> playAgain;
    if (playAgain == 'y' || playAgain == 'Y') {
        cout << "\n" << string(50, '=') << "\n";
        playGameWithLearning();
    }
    else {
        cout << "Thanks for helping the AI learn! Goodbye!\n";
    }
}

int main() {
    srand(time(0));
    playGameWithLearning();
    return 0;
}
