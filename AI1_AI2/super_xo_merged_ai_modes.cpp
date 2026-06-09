
#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <fstream>
#include <random>
#include <ctime>
#include <sstream>

using namespace std;

const int EMPTY = 0;
const int HUMAN = -1; // Human / O, and also AI2 / O in self-play
const int AI = 1;     // Main AI / X, and also AI1 / X in self-play

struct GameResult {
    int result; // From this AI/database perspective: win=1, loss=-1, draw=0
    vector<pair<int, int>> aiMoves; // moves stored as {miniboard, pos}
};

class LearningAI {
public:
    unordered_map<string, pair<int, int>> moveStats; // moveStats[miniboard_pos]={successes,total}
    vector<GameResult> gameHistory;

    double explorationRate = 0.08; // self-play needs some randomness
    int myPlayer;
    int opponentPlayer;
    string name;

private:
    mt19937 rng;

public:
    LearningAI(int player = AI, string aiName = "AI")
        : myPlayer(player),
          opponentPlayer(player == AI ? HUMAN : AI),
          name(aiName),
          rng((unsigned)time(nullptr) ^ (unsigned)clock() ^ (player == AI ? 17 : 91)) {}

    /// Evaluira kolku poeni ima AIot od perspektiva na perspectivePlayer
    static double evaluateState(
        const vector<vector<int>>& boards,
        const vector<int>& metaStatus,
        int perspectivePlayer
    ) {
        int metaResult = checkMetaBoard(metaStatus);

        if (metaResult == perspectivePlayer) return +100000;
        if (metaResult == -perspectivePlayer) return -100000;
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

        for (int b = 0; b < 9; ++b) {
            int status = metaStatus[b];

            if (status == perspectivePlayer) {
                score += WIN_WEIGHT;
            }
            else if (status == -perspectivePlayer) {
                score -= WIN_WEIGHT;
            }
            else {
                for (int i = 0; i < 9; ++i) {
                    if (boards[b][i] == perspectivePlayer) {
                        score += positionWeights[i];
                    }
                    else if (boards[b][i] == -perspectivePlayer) {
                        score -= positionWeights[i];
                    }
                }

                static const int wins[8][3] = {
                    {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
                    {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
                    {0, 4, 8}, {2, 4, 6}
                };

                for (int i = 0; i < 8; ++i) {
                    int a = wins[i][0], d = wins[i][1], c = wins[i][2];
                    int v1 = boards[b][a], v2 = boards[b][d], v3 = boards[b][c];

                    if ((v1 == perspectivePlayer && v2 == perspectivePlayer && v3 == EMPTY) ||
                        (v1 == perspectivePlayer && v3 == perspectivePlayer && v2 == EMPTY) ||
                        (v2 == perspectivePlayer && v3 == perspectivePlayer && v1 == EMPTY)) {
                        score += 50;
                    }

                    if ((v1 == -perspectivePlayer && v2 == -perspectivePlayer && v3 == EMPTY) ||
                        (v1 == -perspectivePlayer && v3 == -perspectivePlayer && v2 == EMPTY) ||
                        (v2 == -perspectivePlayer && v3 == -perspectivePlayer && v1 == EMPTY)) {
                        score -= 50;
                    }
                }
            }
        }

        return score;
    }

    /// Original compatibility overload: evaluates from AI/X perspective
    static double evaluateState(const vector<vector<int>>& boards, const vector<int>& metaStatus) {
        return evaluateState(boards, metaStatus, AI);
    }

    /// Zabelezhuva uspesnost na sekoj poteg
    void learnFromGame(const GameResult& result) {
        gameHistory.push_back(result);

        for (const auto& move : result.aiMoves) {
            string moveKey = to_string(move.first) + "_" + to_string(move.second);

            if (moveStats.find(moveKey) == moveStats.end()) {
                moveStats[moveKey] = {0, 0};
            }

            moveStats[moveKey].second++;

            if (result.result > 0) {
                moveStats[moveKey].first++;
            }
            else if (result.result < 0 && moveStats[moveKey].first > 0) {
                moveStats[moveKey].first--;
            }
        }
    }

    /// Sortiranje po {successes,total}
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

    /// Zachuvuvanje na minatite igri
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

                for (auto& move : g.aiMoves) {
                    file << " " << move.first << " " << move.second;
                }

                file << "\n";
            }

            cout << name << " learning data saved to " << filename << "\n";
            cout << name << " saved " << moveStats.size() << " move statistics and "
                 << gameHistory.size() << " games.\n";
        }
        else {
            cout << "Warning: Could not save learning data to " << filename << "\n";
        }

        file.close();
    }

    /// Prevzema od minatite igri
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

            if (file >> tag >> gameCount) {
                if (tag == "GAME_HISTORY") {
                    for (int i = 0; i < gameCount; ++i) {
                        GameResult gr;
                        int movesInGame;

                        file >> gr.result >> movesInGame;

                        for (int j = 0; j < movesInGame; ++j) {
                            int first, second;

                            file >> first >> second;
                            gr.aiMoves.push_back({first, second});
                        }

                        gameHistory.push_back(gr);
                    }
                }
            }

            cout << name << " loaded " << moveStats.size() << " move statistics and "
                 << gameHistory.size() << " games from " << filename << "\n";
        }
        else {
            cout << "No previous learning data found for " << name << ". Starting from scratch.\n";
            cout << "Learning data will be saved to " << filename << " after games.\n";
        }

        file.close();
    }

    /// Proveri dali nekoj ima pobedeno/ne e gotovo/nereseno
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
                return cells[a];
            }
        }

        for (int i = 0; i < 9; ++i) {
            if (cells[i] == EMPTY) {
                return 0;
            }
        }

        return 2;
    }

    /// Isto za golemata tabla
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
                return metaStatus[a];
            }
        }

        for (int i = 0; i < 9; ++i) {
            if (metaStatus[i] == 0) {
                return 0;
            }
        }

        return 2;
    }

    int minimaxWithLearning(
        vector<vector<int>>& boards,
        vector<int>& metaStatus,
        int currentPlayer,
        int nextBoard,
        int depth,
        int alpha,
        int beta,
        pair<int, int>& bestMove
    ) {
        int metaResult = checkMetaBoard(metaStatus);

        if (metaResult == myPlayer) return +100000;
        if (metaResult == opponentPlayer) return -100000;
        if (metaResult == 2) return 0;

        if (depth == 0) {
            return (int)evaluateState(boards, metaStatus, myPlayer);
        }

        auto moves = generateMoves(boards, metaStatus, nextBoard);

        if (moves.empty()) {
            return (int)evaluateState(boards, metaStatus, myPlayer);
        }

        // Random shuffle first, then sort by learning preference.
        // This keeps MINIMAX, but prevents identical self-play forever.
        shuffle(moves.begin(), moves.end(), rng);

        sort(moves.begin(), moves.end(), [this](const pair<int, int>& a, const pair<int, int>& b) {
            return getMovePreference(a.first, a.second) > getMovePreference(b.first, b.second);
        });

        if (currentPlayer == myPlayer) {
            int maxEval = -1000000000;
            pair<int, int> localBest = moves[0];

            for (auto& mv : moves) {
                int b = mv.first;
                int cell = mv.second;

                boards[b][cell] = currentPlayer;
                int oldStatus = metaStatus[b];

                updateSmallStatus(boards, metaStatus, b);

                int nextB = (metaStatus[cell] == 0) ? cell : -1;

                pair<int, int> tempMove(-1, -1);

                int eval = minimaxWithLearning(
                    boards,
                    metaStatus,
                    -currentPlayer,
                    nextB,
                    depth - 1,
                    alpha,
                    beta,
                    tempMove
                );

                boards[b][cell] = EMPTY;
                metaStatus[b] = oldStatus;

                if (eval > maxEval) {
                    maxEval = eval;
                    localBest = mv;
                }

                alpha = max(alpha, eval);

                if (beta <= alpha) {
                    break;
                }
            }

            bestMove = localBest;
            return maxEval;
        }
        else {
            int minEval = 1000000000;
            pair<int, int> localBest = moves[0];

            for (auto& mv : moves) {
                int b = mv.first;
                int cell = mv.second;

                boards[b][cell] = currentPlayer;
                int oldStatus = metaStatus[b];

                updateSmallStatus(boards, metaStatus, b);

                int nextB = (metaStatus[cell] == 0) ? cell : -1;

                pair<int, int> tempMove(-1, -1);

                int eval = minimaxWithLearning(
                    boards,
                    metaStatus,
                    -currentPlayer,
                    nextB,
                    depth - 1,
                    alpha,
                    beta,
                    tempMove
                );

                boards[b][cell] = EMPTY;
                metaStatus[b] = oldStatus;

                if (eval < minEval) {
                    minEval = eval;
                    localBest = mv;
                }

                beta = min(beta, eval);

                if (beta <= alpha) {
                    break;
                }
            }

            bestMove = localBest;
            return minEval;
        }
    }

    /// Generiranje na site moves
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
                if (metaStatus[b] != 0) {
                    continue;
                }

                for (int cell = 0; cell < 9; ++cell) {
                    if (boards[b][cell] == EMPTY) {
                        moves.emplace_back(b, cell);
                    }
                }
            }
        }

        return moves;
    }

    /// Setira status na metaBoard
    static void updateSmallStatus(vector<vector<int>>& boards, vector<int>& metaStatus, int b) {
        metaStatus[b] = checkSmallBoard(boards[b]);
    }

    pair<int, int> exploreMove(const vector<pair<int, int>>& moves) {
        if (moves.empty()) {
            return {-1, -1};
        }

        uniform_int_distribution<int> dist(0, (int)moves.size() - 1);
        return moves[dist(rng)];
    }

    pair<int, int> chooseMove(
        vector<vector<int>>& boards,
        vector<int>& metaStatus,
        int nextBoard,
        int searchDepth
    ) {
        auto moves = generateMoves(boards, metaStatus, nextBoard);

        if (moves.empty()) {
            return {-1, -1};
        }

        uniform_real_distribution<double> realDist(0.0, 1.0);

        if (realDist(rng) < explorationRate) {
            return exploreMove(moves);
        }

        shuffle(moves.begin(), moves.end(), rng);

        vector<pair<pair<int, int>, int>> scoredMoves;
        scoredMoves.reserve(moves.size());

        for (auto& mv : moves) {
            int b = mv.first;
            int cell = mv.second;

            boards[b][cell] = myPlayer;
            int oldStatus = metaStatus[b];

            updateSmallStatus(boards, metaStatus, b);

            int metaResult = checkMetaBoard(metaStatus);

            if (metaResult == myPlayer) {
                boards[b][cell] = EMPTY;
                metaStatus[b] = oldStatus;
                return mv;
            }

            int nextB = (metaStatus[cell] == 0) ? cell : -1;

            pair<int, int> tmp(-1, -1);

            int eval = minimaxWithLearning(
                boards,
                metaStatus,
                opponentPlayer,
                nextB,
                searchDepth - 1,
                numeric_limits<int>::min(),
                numeric_limits<int>::max(),
                tmp
            );

            // Small learned preference bonus, so saved database affects choices.
            eval += (int)(getMovePreference(b, cell) * 100);

            scoredMoves.push_back({mv, eval});

            boards[b][cell] = EMPTY;
            metaStatus[b] = oldStatus;
        }

        sort(scoredMoves.begin(), scoredMoves.end(),
            [](const pair<pair<int, int>, int>& a, const pair<pair<int, int>, int>& b) {
                return a.second > b.second;
            }
        );

        // Pick randomly from several close-to-best moves, not always exact same one.
        int bestScore = scoredMoves[0].second;
        vector<pair<int, int>> goodMoves;

        for (int i = 0; i < (int)scoredMoves.size(); ++i) {
            if (scoredMoves[i].second >= bestScore - 80) {
                goodMoves.push_back(scoredMoves[i].first);
            }

            if ((int)goodMoves.size() >= 4) {
                break;
            }
        }

        if (goodMoves.empty()) {
            return scoredMoves[0].first;
        }

        uniform_int_distribution<int> dist(0, (int)goodMoves.size() - 1);
        return goodMoves[dist(rng)];
    }
};

void printSmallBoard(const vector<int>& board) {
    for (int i = 0; i < 9; ++i) {
        char c = (board[i] == AI) ? 'X'
            : (board[i] == HUMAN) ? 'O'
            : '.';

        cout << c;

        if (i % 3 == 2 && i != 8) {
            cout << "\n";
        }
        else if (i % 3 != 2) {
            cout << " ";
        }
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

                    if (col != 2) {
                        cout << " ";
                    }
                }

                if (blockCol != 2) {
                    cout << " | ";
                }
            }

            cout << "\n";
        }

        if (blockRow != 2) {
            cout << "---------------------\n";
        }
    }
}

string moveListToString(const vector<pair<int, int>>& moves) {
    stringstream ss;

    for (size_t i = 0; i < moves.size(); ++i) {
        if (i > 0) {
            ss << " ";
        }

        ss << moves[i].first << "," << moves[i].second;
    }

    return ss.str();
}

vector<pair<int, int>> combineMoves(
    const vector<pair<int, int>>& a,
    const vector<pair<int, int>>& b
) {
    vector<pair<int, int>> out = a;

    for (auto& move : b) {
        out.push_back(move);
    }

    return out;
}

void appendGameToDatabaseFile(
    const string& filename,
    int gameNumber,
    int winner,
    const vector<pair<int, int>>& ai1Moves,
    const vector<pair<int, int>>& ai2Moves
) {
    ofstream file(filename, ios::app);

    if (!file.is_open()) {
        cout << "Warning: Could not open " << filename << " for game database output.\n";
        return;
    }

    file << "GAME " << gameNumber << "\n";
    file << "WINNER " << winner << " ";

    if (winner == AI) {
        file << "AI1_X\n";
    }
    else if (winner == HUMAN) {
        file << "AI2_O\n";
    }
    else {
        file << "DRAW\n";
    }

    file << "AI1_X_MOVES " << ai1Moves.size() << " " << moveListToString(ai1Moves) << "\n";
    file << "AI2_O_MOVES " << ai2Moves.size() << " " << moveListToString(ai2Moves) << "\n";
    file << "END_GAME\n\n";

    file.close();
}

void addSelfPlayGameToOriginalDatabase(
    LearningAI& originalDatabaseAI,
    int winner,
    const vector<pair<int, int>>& ai1Moves,
    const vector<pair<int, int>>& ai2Moves
) {
    GameResult originalResult;

    if (winner == AI) {
        // AI1 won, so AI1 is treated as the original AI.
        // AI2 is treated as the human/opponent.
        originalResult.result = 1;
        originalResult.aiMoves = ai1Moves;
        originalDatabaseAI.learnFromGame(originalResult);
    }
    else if (winner == HUMAN) {
        // AI2 won, so AI2 is treated as the original AI.
        // AI1 is treated as the human/opponent.
        originalResult.result = 1;
        originalResult.aiMoves = ai2Moves;
        originalDatabaseAI.learnFromGame(originalResult);
    }
    else {
        // Draw: no winner, so store both move sets as neutral data.
        originalResult.result = 0;
        originalResult.aiMoves = combineMoves(ai1Moves, ai2Moves);
        originalDatabaseAI.learnFromGame(originalResult);
    }
}

void playHumanVsAI() {
    LearningAI ai(AI, "Original_AI_X");
    ai.explorationRate = 0.01;

    cout << "Learning file will be saved in current directory.\n";
    cout << "Look for 'ai_learning.dat' in the same folder as your executable.\n\n";

    ai.loadFromFile("ai_learning.dat");

    vector<vector<int>> boards(9, vector<int>(9, EMPTY));
    vector<int> metaStatus(9, 0);
    int nextBoard = -1;
    int currentPlayer = HUMAN;

    GameResult currentGame;
    vector<pair<int, int>> gameMovesForLearning;

    int searchDepth;
    cout << "Enter AI minimax depth, recommended 5-7 for Human vs AI: ";
    cin >> searchDepth;

    if (searchDepth < 1) {
        searchDepth = 1;
    }

    cout << "\nUltimate Tic-Tac-Toe with Learning AI!\n";
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
                    if (metaStatus[i] == 0) {
                        cout << i << " ";
                    }
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

            pair<int, int> aiMove = ai.chooseMove(
                boards,
                metaStatus,
                nextBoard,
                searchDepth
            );

            if (aiMove.first == -1 || aiMove.second == -1) {
                cout << "No moves available for AI!\n";
                currentGame.result = 0;
                break;
            }

            int b = aiMove.first;
            int cell = aiMove.second;

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

    currentGame.aiMoves = gameMovesForLearning;

    cout << "AI is learning from this game...\n";
    ai.learnFromGame(currentGame);
    ai.saveToFile("ai_learning.dat");

    cout << "Game completed. AI has learned from "
         << gameMovesForLearning.size() << " moves in this game.\n";
}

void playSelfPlayGames() {
    LearningAI ai1(AI, "AI1_X");
    LearningAI ai2(HUMAN, "AI2_O");
    LearningAI originalDatabaseAI(AI, "Original_AI_Database");

    cout << "Ultimate Tic-Tac-Toe Self-Play with Learning AI!\n";
    cout << "AI1 is X. AI2 is O.\n";
    cout << "Both players use minimax + learning stats + exploration.\n";
    cout << "The winner of each self-play game is also saved into ai_learning.dat as the original AI.\n\n";

    ai1.loadFromFile("ai1_learning.dat");
    ai2.loadFromFile("ai2_learning.dat");
    originalDatabaseAI.loadFromFile("ai_learning.dat");

    int numberOfGames;
    cout << "\nHow many self-play games should the AIs play? ";
    cin >> numberOfGames;

    if (numberOfGames <= 0) {
        cout << "Number of games must be positive.\n";
        return;
    }

    int searchDepth;
    cout << "Enter minimax depth, recommended 3-5 for many games, 6-7 for slower stronger play: ";
    cin >> searchDepth;

    if (searchDepth < 1) {
        searchDepth = 1;
    }

    int showBoardsInput;
    cout << "Show every board while playing? 1=yes, 0=no recommended for database: ";
    cin >> showBoardsInput;

    bool showBoards = (showBoardsInput == 1);

    int ai1Wins = 0;
    int ai2Wins = 0;
    int draws = 0;

    ofstream clearDb("self_play_games.txt");
    clearDb << "Ultimate Tic-Tac-Toe self-play database\n";
    clearDb << "Format: board,cell pairs for every AI\n";
    clearDb << "Also note: winner moves are added into ai_learning.dat as original AI data.\n\n";
    clearDb.close();

    for (int game = 1; game <= numberOfGames; ++game) {
        vector<vector<int>> boards(9, vector<int>(9, EMPTY));
        vector<int> metaStatus(9, 0);

        int nextBoard = -1;
        int currentPlayer = AI;

        vector<pair<int, int>> ai1Moves;
        vector<pair<int, int>> ai2Moves;

        while (true) {
            int metaResult = LearningAI::checkMetaBoard(metaStatus);

            if (metaResult != 0) {
                if (metaResult == AI) {
                    ai1Wins++;
                }
                else if (metaResult == HUMAN) {
                    ai2Wins++;
                }
                else {
                    draws++;
                }

                GameResult resultForAI1;
                resultForAI1.result = (metaResult == AI) ? 1 : (metaResult == HUMAN ? -1 : 0);
                resultForAI1.aiMoves = ai1Moves;

                GameResult resultForAI2;
                resultForAI2.result = (metaResult == HUMAN) ? 1 : (metaResult == AI ? -1 : 0);
                resultForAI2.aiMoves = ai2Moves;

                ai1.learnFromGame(resultForAI1);
                ai2.learnFromGame(resultForAI2);

                // This is the important merged part:
                // The winning AI is saved into the original ai_learning.dat database as "AI".
                // The losing AI is treated as the human/opponent because only winner moves are stored as aiMoves.
                addSelfPlayGameToOriginalDatabase(
                    originalDatabaseAI,
                    metaResult,
                    ai1Moves,
                    ai2Moves
                );

                appendGameToDatabaseFile(
                    "self_play_games.txt",
                    game,
                    metaResult,
                    ai1Moves,
                    ai2Moves
                );

                if (showBoards) {
                    cout << "\nFinal board for game " << game << ":\n";
                    printUltimate(boards);
                    cout << "\n";
                }

                break;
            }

            LearningAI& playerAI = (currentPlayer == AI) ? ai1 : ai2;

            pair<int, int> move = playerAI.chooseMove(
                boards,
                metaStatus,
                nextBoard,
                searchDepth
            );

            if (move.first == -1 || move.second == -1) {
                draws++;

                GameResult resultForAI1;
                resultForAI1.result = 0;
                resultForAI1.aiMoves = ai1Moves;

                GameResult resultForAI2;
                resultForAI2.result = 0;
                resultForAI2.aiMoves = ai2Moves;

                ai1.learnFromGame(resultForAI1);
                ai2.learnFromGame(resultForAI2);

                addSelfPlayGameToOriginalDatabase(
                    originalDatabaseAI,
                    2,
                    ai1Moves,
                    ai2Moves
                );

                appendGameToDatabaseFile(
                    "self_play_games.txt",
                    game,
                    2,
                    ai1Moves,
                    ai2Moves
                );

                break;
            }

            int b = move.first;
            int cell = move.second;

            boards[b][cell] = currentPlayer;
            LearningAI::updateSmallStatus(boards, metaStatus, b);

            if (currentPlayer == AI) {
                ai1Moves.push_back(move);
            }
            else {
                ai2Moves.push_back(move);
            }

            if (showBoards) {
                cout << "\nGame " << game << " - "
                     << ((currentPlayer == AI) ? "AI1_X" : "AI2_O")
                     << " plays board " << b << ", cell " << cell << "\n";
                printUltimate(boards);
                cout << "\n";
            }

            if (metaStatus[cell] == 0) {
                nextBoard = cell;
            }
            else {
                nextBoard = -1;
            }

            currentPlayer = -currentPlayer;
        }

        if (!showBoards) {
            cout << "Game " << game << "/" << numberOfGames
                 << " done. Score: AI1_X=" << ai1Wins
                 << ", AI2_O=" << ai2Wins
                 << ", Draws=" << draws << "\n";
        }
    }

    cout << "\nSelf-play finished.\n";
    cout << "Final score after " << numberOfGames << " games:\n";
    cout << "AI1_X wins: " << ai1Wins << "\n";
    cout << "AI2_O wins: " << ai2Wins << "\n";
    cout << "Draws: " << draws << "\n\n";

    ai1.saveToFile("ai1_learning.dat");
    ai2.saveToFile("ai2_learning.dat");
    originalDatabaseAI.saveToFile("ai_learning.dat");

    cout << "Raw played-game database saved to self_play_games.txt\n";
    cout << "AI1 learning file: ai1_learning.dat\n";
    cout << "AI2 learning file: ai2_learning.dat\n";
    cout << "Original AI learning file also updated: ai_learning.dat\n";
}

int main() {
    srand((unsigned)time(0));

    while (true) {
        cout << "\n========================================\n";
        cout << "Ultimate Tic-Tac-Toe / Super XO\n";
        cout << "1. Play Human vs AI\n";
        cout << "2. Run AI1 vs AI2 self-play\n";
        cout << "3. Exit\n";
        cout << "Choose mode: ";

        int choice;
        cin >> choice;

        if (choice == 1) {
            playHumanVsAI();
        }
        else if (choice == 2) {
            playSelfPlayGames();
        }
        else if (choice == 3) {
            cout << "Goodbye!\n";
            break;
        }
        else {
            cout << "Invalid choice. Try again.\n";
        }
    }

    return 0;
}
