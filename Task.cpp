#include <iostream>
#include <fstream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <ctime>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <sstream>
#include <map> // Include map header

using namespace std;

class TrafficData {
public:
    string timestamp;
    vector<pair<string, int>> traffic_light_data;

    TrafficData(string ts) : timestamp(ts) {}
};

class TrafficBuffer {
private:
    queue<TrafficData> buffer;
    mutex mtx;
    condition_variable not_full;
    condition_variable not_empty;
    int max_size;

public:
    TrafficBuffer(int size) : max_size(size) {}

    void put(const TrafficData& data) {
        unique_lock<mutex> lock(mtx);
        not_full.wait(lock, [this] { return buffer.size() < max_size; });
        buffer.push(data);
        lock.unlock();
        not_empty.notify_one();
    }

    TrafficData get() {
        unique_lock<mutex> lock(mtx);
        not_empty.wait(lock, [this] { return !buffer.empty(); });
        TrafficData data = buffer.front();
        buffer.pop();
        lock.unlock();
        not_full.notify_one();
        return data;
    }
};

class TrafficProducer {
private:
    TrafficBuffer& buffer;
    bool running;

public:
    TrafficProducer(TrafficBuffer& buf) : buffer(buf), running(true) {}

    void produceFromFile(const string& filename) {
        ifstream file(filename);
        if (file.is_open()) {
            string line;
            while (getline(file, line)) {
                istringstream iss(line);
                string timestamp;
                iss >> timestamp;
                TrafficData data(timestamp);
                string light_id;
                int cars_passed;
                while (iss >> light_id >> cars_passed) {
                    data.traffic_light_data.push_back({light_id, cars_passed});
                }
                buffer.put(data);
                cout << "Consumed: Timestamp: " << data.timestamp << " Light Data: ";
                for (const auto& light_data : data.traffic_light_data) {
                    cout << "ID: " << light_data.first << " Cars Passed: " << light_data.second << " ";
                }
                cout << endl;
            }
            file.close();
        } else {
            cerr << "Unable to open file." << endl;
        }
    }

    void stop() {
        running = false;
    }
};

class TrafficConsumer {
private:
    TrafficBuffer& buffer;
    bool running;
    map<string, vector<pair<string, int>>> max_cars_passed; // Map to store the maximum cars passed for each timestamp

public:
    TrafficConsumer(TrafficBuffer& buf) : buffer(buf), running(true) {}

    void consume() {
        while (running) {
            TrafficData data = buffer.get();
            updateMaxCarsPassed(data);
            cout << "Consumed: Timestamp: " << data.timestamp << " Light Data: ";
            for (const auto& light_data : data.traffic_light_data) {
                cout << "ID: " << light_data.first << " Cars Passed: " << light_data.second << " ";
            }
            cout << endl;
        }
    }

    void updateMaxCarsPassed(const TrafficData& data) {
        string timestamp = data.timestamp;
        if (max_cars_passed.find(timestamp) == max_cars_passed.end()) {
            max_cars_passed[timestamp] = data.traffic_light_data;
        } else {
            for (const auto& light_data : data.traffic_light_data) {
                for (auto& existing_light_data : max_cars_passed[timestamp]) {
                    if (existing_light_data.first == light_data.first) {
                        existing_light_data.second = max(existing_light_data.second, light_data.second);
                        break;
                    }
                }
            }
        }
    }

    void printMaxCarsPassed() {
        cout << "Maximum cars passed for each timestamp:" << endl;
        for (const auto& entry : max_cars_passed) {
            cout << "Timestamp: " << entry.first << " Light Data: ";
            for (const auto& light_data : entry.second) {
                cout << "ID: " << light_data.first << " Max Cars Passed: " << light_data.second << " ";
            }
            cout << endl;
        }
    }

    void stop() {
        running = false;
    }
};

int main() {
    srand(time(nullptr));
    int buffer_size = 10;

    TrafficBuffer buffer(buffer_size);

    TrafficProducer producer(buffer);
    producer.produceFromFile("test_data.txt");

    TrafficConsumer consumer(buffer);
    consumer.consume();
    consumer.printMaxCarsPassed();

    return 0;
}
