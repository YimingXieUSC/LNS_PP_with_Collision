/* MAPF LNS with PP collision first approach driver */

#include <iostream>
#include <vector>
#include <iterator>
#include <set>
#include <random>

using namespace std;

// to generate random number
random_device rd;     // only used once to initialise (seed) engine
mt19937 rng(rd());    // random-number engine used (Mersenne-Twister in this case)

int generate_random(int minimum, int maximum){
    std::uniform_int_distribution<int> uni(minimum,maximum); // guaranteed unbiased
    int random_integer = uni(rng);
    return random_integer;
}

class location{
    public: 
    int x;
    int y;
    location(int xx, int yy){
        x = xx;
        y = yy;
    }
    bool operator<(location loc2) const
    {
        return x < loc2.x ? true : y < loc2.y;
    }
    bool operator==(const location loc2){
        return x == loc2.x && y == loc2.y;
    }

};

// generateAgents
// @agentNum: the quantity of the agents
// @col: the width of the map
// @row: the height of the map
// return: a vector of int pairs indicating the s and g for each agent
vector<pair<location, location> > generateAgents(int agentNum, int col, int row){
    vector<pair<location, location> > result;
    set<location> locationSet;
    if(agentNum * 2 > col * col){
        return result;
    }
    else{
        for(int i = 0; i < agentNum; i++){
            location loc1 = location(generate_random(0, col - 1), generate_random(0, row - 1));
            location loc2 =location(generate_random(0, col - 1), generate_random(0, row - 1));
            while((locationSet.find(loc1) != locationSet.end() && locationSet.find(loc2) != locationSet.end()) || loc1 == loc2){
                loc1 = location(generate_random(0, col - 1), generate_random(0, row - 1));
                loc2 = location(generate_random(0, col - 1), generate_random(0, row - 1));
            }  
            locationSet.insert(loc1);
            locationSet.insert(loc2);
            result.push_back(make_pair(loc1, loc2));
        }
        return result;
    }
}



// given: G = (V, E), set of M agents -> A = {a_1, a_m} , a start vertex s in V for each agent
// a target vertex g in B for each agent
int main ()
{
    vector<pair<location, location> > haha = generateAgents(8, 5, 6);
    for(int i = 0; i < haha.size(); i++){
        cout << i << "th pair: " << endl;
        cout << haha[i].first.x  << " " << haha[i].first.y << endl;
        cout << haha[i].second.x << " " << haha[i].second.y << endl;
    }
    // 1. find the shortest paths for all agents
    // 2. sort the positions (x, y) by the # of collisions(result will be a table of int)
    // 3. given each agent a specific weight based on their shortest
    // 4. 


}