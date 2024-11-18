#include "api.h"
#include "utils.h"
#include <iostream>
#include <random>
#include <spdlog/spdlog.h>
#include <string>
#include <tuple>
#include <utility>
#include <queue>
#include <set>

using namespace cycles;

class BotClient {
    Connection connection;
    std::string name;
    GameState state;
    Player my_player;
    std::mt19937 rng;
    int previousDirection = -1;
    int fallbackCounter = 0;

    bool is_valid_move(Direction direction) {
        auto new_pos = my_player.position + getDirectionVector(direction);
        return state.isInsideGrid(new_pos) && state.getGridCell(new_pos) == 0;
    }

    Direction findBestMove(std::vector<std::pair<Direction, int>> &moves) {
        if (moves.empty()) return Direction::north; // Default fallback
        std::sort(moves.begin(), moves.end(), [](auto &a, auto &b) {
            return a.second > b.second;
        });
        return moves.front().first;
    }

    Direction findSafeDirection() {
        std::vector<std::pair<Direction, int>> moves;

        for (auto dir : {Direction::north, Direction::east, Direction::south, Direction::west}) {
            if (is_valid_move(dir)) {
                sf::Vector2i new_pos = my_player.position + getDirectionVector(dir);
                int accessibleArea = calculateAccessibleArea(new_pos);
                moves.emplace_back(dir, accessibleArea);
            }
        }

        return findBestMove(moves);
    }

    Direction decideAggressiveMove(const sf::Vector2i &target, const sf::Vector2i &predictedHead) {
        std::vector<std::pair<Direction, int>> moves;
        for (auto dir : {Direction::north, Direction::east, Direction::south, Direction::west}) {
            sf::Vector2i new_pos = my_player.position + getDirectionVector(dir);
            if (is_valid_move(dir)) {
                int distance = abs(new_pos.x - target.x) + abs(new_pos.y - target.y);
                int area = calculateAccessibleArea(new_pos);
                int score = area - distance; // Balances safety with aggression
                moves.emplace_back(dir, score);
            }
        }
        return findBestMove(moves);
    }

    sf::Vector2i predictOpponentMove(const sf::Vector2i &opponentHead) {
        sf::Vector2i bestMove = opponentHead;
        int maxArea = 0;

        for (auto d : {Direction::north, Direction::east, Direction::south, Direction::west}) {
            sf::Vector2i candidateMove = opponentHead + getDirectionVector(d);
            if (state.isInsideGrid(candidateMove) && state.getGridCell(candidateMove) == 0) {
                int area = calculateAccessibleArea(candidateMove);
                if (area > maxArea) {
                    maxArea = area;
                    bestMove = candidateMove;
                }
            }
        }
        return bestMove;
    }

    int calculateAccessibleArea(const sf::Vector2i &start) {
        std::queue<sf::Vector2i> toVisit;
        std::set<sf::Vector2i> visited;
        toVisit.push(start);
        int area = 0;

        while (!toVisit.empty()) {
            sf::Vector2i current = toVisit.front();
            toVisit.pop();
            if (visited.count(current) || !state.isInsideGrid(current) || state.getGridCell(current) != 0) {
                continue;
            }
            visited.insert(current);
            area++;

            for (auto d : {Direction::north, Direction::east, Direction::south, Direction::west}) {
                sf::Vector2i neighbor = current + getDirectionVector(d);
                toVisit.push(neighbor);
            }
        }
        return area;
    }

    Direction decideMove() {
        auto [nearestHead, nearestOpponent] = findNearestOpponentHead();

        if (nearestHead == sf::Vector2i{-1, -1}) {
            return findSafeDirection();
        }

        sf::Vector2i predictedMove = predictOpponentMove(nearestHead);
        return decideAggressiveMove(predictedMove, nearestHead);
    }

    void receiveGameState() {
        state = connection.receiveGameState();
        for (const auto &player : state.players) {
            if (player.name == name) {
                my_player = player;
                break;
            }
        }
    }

    void sendMove() {
        auto move = decideMove();
        previousDirection = getDirectionValue(move);
        connection.sendMove(move);
    }

public:
    BotClient(const std::string &botName) : name(botName) {
        std::random_device rd;
        rng.seed(rd());
        connection.connect(name);

        if (!connection.isActive()) {
            spdlog::critical("{}: Connection failed", name);
            exit(1);
        }
    }

    void run() {
        while (connection.isActive()) {
            receiveGameState();
            sendMove();
        }
    }
};

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <bot_name>" << std::endl;
        return 1;
    }

    #if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_TRACE
    spdlog::set_level(spdlog::level::debug);
    #endif
    std::string botName = argv[1];
    BotClient bot(botName);
    bot.run();
    return 0;
}
