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


// The important takeaway here is the strategy of moving in a conservative manner 
// and avoid collisions instead of being agressive and cut off other players

class BotClient {
    Connection connection; 
    std::string name;      
    GameState state;       
    Player my_player;      
    std::mt19937 rng;      
    int previousDirection = -1; 
    int fallbackCounter = 0;    



    // Check if a move is valid by ensuring the new position is inside the grid and unoccupied
    bool is_valid_move(Direction direction) {
        auto new_pos = my_player.position + getDirectionVector(direction);
        return state.isInsideGrid(new_pos) && state.getGridCell(new_pos) == 0;
    }

    // Return the best move from a set of possible moves 
    Direction findBestMove(std::vector<std::pair<Direction, int>> &moves) {
        if (moves.empty()) return Direction::north; // Default fallback
        std::sort(moves.begin(), moves.end(), [](auto &a, auto &b) {
            return a.second > b.second; // Sort by score, highest first
        });
        return moves.front().first; // Return the move with the highest score
    }

    // Return the safest direction by considering immediate accessible area
    Direction findSafeDirection() {
        std::vector<std::pair<Direction, int>> moves;

        for (auto dir : {Direction::north, Direction::east, Direction::south, Direction::west}) {
            if (is_valid_move(dir)) { // Only consider valid moves
                sf::Vector2i new_pos = my_player.position + getDirectionVector(dir);
                int accessibleArea = calculateAccessibleArea(new_pos); // BFS 
                moves.emplace_back(dir, accessibleArea); 
            }
        }

        return findBestMove(moves); // Choose the move with the largest safe area available
    }

    // This function calculates the area that can be reached from a given position
    int calculateAccessibleArea(const sf::Vector2i &start) {
        std::queue<sf::Vector2i> toVisit;  //Using a queue for the BFS for its FIFO property as we go through the area sequentially
        std::set<sf::Vector2i> visited;   // Tracks visited cells
        toVisit.push(start);
        int area = 0;

        // While loop to traverse the area 
        while (!toVisit.empty()) {
            sf::Vector2i current = toVisit.front();
            toVisit.pop();
            // Skip cells that are already visited, on the wall, or occupied 
            if (visited.count(current) || !state.isInsideGrid(current) || state.getGridCell(current) != 0) {
                continue;
            }
            visited.insert(current); // Marked as visited
            area++; // Increment count

            // Add all neighboring cells for BFS traversal
            for (auto d : {Direction::north, Direction::east, Direction::south, Direction::west}) {
                sf::Vector2i neighbor = current + getDirectionVector(d);
                toVisit.push(neighbor);
            }
        }
        return area; // Total cells available
    }

    // Decide the next move based on the current state 
    Direction decideMove() {
        return findSafeDirection();  // Thi part of ensures that safety is met over agressiveness
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
