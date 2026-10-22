// Part 1 (C++ / OOP): market data builder
// Downloads today's USD-based reference rates, works out the rate between
// every pair of currencies, adds our fee/spread assumptions and saves
// everything to edges.csv for the C program to analyse.
//
// Usage:
//   ./market_builder                live data only
//   ./market_builder --demo         live data + two clearly labelled fake mispricings
//   ./market_builder --file x.json  use a saved API response (handy for offline testing)

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace std;

struct Currency {
    string code;
    double unitsPerUSD;
};

// Gets the raw JSON and pulls numbers out of it.
class LiveRateProvider {
public:
    string fetchRates() const {
        FILE *pipe = popen("curl -fsSL --max-time 20 https://open.er-api.com/v6/latest/USD", "r");
        if (!pipe) return "";
        string response;
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) response += buffer;
        int status = pclose(pipe);
        return status == 0 ? response : "";
    }

    string readFile(const string &path) const {
        ifstream in(path);
        stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    // Looks up one currency inside the "rates" object only.
    // (Searching the whole text would hit "base_code":"USD" first and fail.)
    bool readRate(const string &json, const string &code, double &rate) const {
        size_t ratesPos = json.find("\"rates\"");
        if (ratesPos == string::npos) return false;

        string key = "\"" + code + "\"";
        size_t pos = json.find(key, ratesPos);
        if (pos == string::npos) return false;
        pos = json.find(':', pos + key.size());
        if (pos == string::npos) return false;

        char *end = nullptr;
        rate = strtod(json.c_str() + pos + 1, &end);
        return end != json.c_str() + pos + 1 && rate > 0.0;
    }

    string lastUpdated(const string &json) const {
        size_t pos = json.find("\"time_last_update_utc\"");
        if (pos == string::npos) return "not supplied";
        size_t start = json.find('"', json.find(':', pos));
        size_t end = json.find('"', start + 1);
        if (start == string::npos || end == string::npos) return "not supplied";
        return json.substr(start + 1, end - start - 1);
    }
};

// Our assumed trading costs (the API does not provide these).
class MarketConditions {
private:
    double feePercent;
    double spreadPercent;

public:
    MarketConditions(double fee = 0.10, double spread = 0.10)
        : feePercent(fee), spreadPercent(spread) {}

    double fee() const { return feePercent; }
    double spread() const { return spreadPercent; }

    double effectiveRate(double referenceRate) const {
        return referenceRate * (1.0 - feePercent / 100.0) * (1.0 - spreadPercent / 100.0);
    }
};

// Holds the currencies and writes the graph out.
class MarketDataBuilder {
private:
    vector<Currency> currencies;
    vector<string> supported = {"USD", "EUR", "GBP", "INR", "JPY", "CAD", "AUD", "CHF", "SGD"};
    bool demoMode = false;

    // Only used in --demo mode: pretend one venue quotes a few routes too generously.
    double demoFactor(const string &from, const string &to) const {
        if (!demoMode) return 1.0;
        if (from == "INR" && to == "GBP") return 1.015;  // +1.5%
        if (from == "EUR" && to == "JPY") return 1.020;  // +2.0%
        return 1.0;
    }

public:
    void setDemoMode(bool on) { demoMode = on; }

    bool load(const string &json, const LiveRateProvider &provider) {
        currencies.clear();
        for (const string &code : supported) {
            double rate = 0.0;
            if (!provider.readRate(json, code, rate)) {
                cerr << "API response did not contain a valid rate for " << code << ".\n";
                return false;
            }
            currencies.push_back({code, rate});
        }
        return true;
    }

    void showRates() const {
        cout << "\nREFERENCE RATES (units of currency per 1 USD)\n";
        cout << fixed << setprecision(6);
        for (const Currency &c : currencies)
            cout << "  " << left << setw(6) << c.code << right << setw(14) << c.unitsPerUSD << '\n';
    }

    bool writeEdges(const MarketConditions &market, const string &filename) const {
        ofstream out(filename);
        if (!out) {
            cerr << "Could not create " << filename << ". Check folder permissions.\n";
            return false;
        }
        out << "from,to,rate,fee_percent,spread_percent,effective_rate,cost\n";
        out << setprecision(12);
        for (size_t i = 0; i < currencies.size(); i++) {
            for (size_t j = 0; j < currencies.size(); j++) {
                if (i == j) continue;
                double cross = currencies[j].unitsPerUSD / currencies[i].unitsPerUSD;
                cross *= demoFactor(currencies[i].code, currencies[j].code);
                double effective = market.effectiveRate(cross);
                out << currencies[i].code << ',' << currencies[j].code << ',' << cross << ','
                    << market.fee() << ',' << market.spread() << ',' << effective << ','
                    << -log(effective) << '\n';
            }
        }
        return true;
    }

    size_t size() const { return currencies.size(); }
};

int main(int argc, char **argv) {
    bool demo = false;
    string file;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--demo") demo = true;
        else if (arg == "--file" && i + 1 < argc) file = argv[++i];
    }

    cout << "CURRENCY ARBITRAGE - MARKET DATA BUILDER (C++ / OOP)\n";

    LiveRateProvider provider;
    string json;
    if (file.empty()) {
        cout << "Downloading the latest reference-rate snapshot...\n";
        json = provider.fetchRates();
    } else {
        cout << "Reading saved snapshot from " << file << "...\n";
        json = provider.readFile(file);
    }
    if (json.empty()) {
        cerr << "\nERROR: Could not get rates. Check your internet connection and try again.\n";
        cerr << "The program stops rather than using made-up sample data.\n";
        return 1;
    }

    cout << "Source: https://open.er-api.com/v6/latest/USD\n";
    cout << "Provider update: " << provider.lastUpdated(json) << '\n';
    cout << "Note: these are reference rates, not executable bid/ask quotes.\n";

    MarketDataBuilder builder;
    builder.setDemoMode(demo);
    if (!builder.load(json, provider)) return 1;

    MarketConditions market(0.10, 0.10);  // change these to try other cost assumptions
    builder.showRates();

    cout << "\nCOST ASSUMPTIONS (ours, not from the API)\n";
    cout << "  Fee per conversion:    " << market.fee() << "%\n";
    cout << "  Spread per conversion: " << market.spread() << "%\n";

    if (demo) {
        cout << "\n*** DEMO MODE: INR->GBP boosted by 1.5% and EUR->JPY by 2.0% ***\n";
        cout << "*** to imitate a venue with generous quotes. NOT real market data. ***\n";
    }

    if (!builder.writeEdges(market, "edges.csv")) return 1;
    cout << "\nSaved edges.csv with " << builder.size() * (builder.size() - 1)
         << " directed routes. Next: run the C program.\n";
    return 0;
}
