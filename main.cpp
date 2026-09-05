#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <cstdlib>
#include <array>
#include <filesystem>
#include <memory>
#include <cstdio>
#include <chrono>
#include <map>
#include <fstream>
#include <sstream>

#include <httplib.h>

#include <boost/asio.hpp>

namespace boost
{
    namespace asio
    {
        using io_service = io_context;
    }
}

#include <tgbot/tgbot.h>

using namespace std;

struct FileGuard
{
    string path;
    FileGuard(string p) : path(move(p)) {}
    ~FileGuard()
    {
        if (filesystem::exists(path))
        {
            filesystem::remove(path);
        }
    }
};

long long get_video_size(string url)
{
    string command = "yt-dlp --impersonate chrome "
                     "-f \"bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best\" "
                     "--print filesize_approx --no-playlist \"" +
                     url + "\" 2>/dev/null";

    array<char, 128> buffer;
    string result;
    using PipeCloser = int (*)(FILE *);
    unique_ptr<FILE, PipeCloser> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe)
        return -1;

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }
    if (result.empty())
        return -1;

    try
    {
        return stoll(result);
    }
    catch (...)
    {
        return -1;
    }
}

int main()
{
    const char *token_env = getenv("BOT_TOKEN");
    string token;
    if (token_env)
    {
        token = token_env;
    }
    else
    {
        cerr << "BOT_TOKEN не знайдено!" << endl;
        return 1;
    }

    const char *port_env = getenv("PORT");
    int port = port_env ? atoi(port_env) : 8080;

    TgBot::Bot bot(token);
    map<int64_t, string> user_actions;

    bot.getEvents().onCommand("start", [&bot](TgBot::Message::Ptr message)
                              { bot.getApi().sendMessage(message->chat->id, "hello i am a bot URL tool who will work with your video"); });

    bot.getEvents().onCommand("video", [&bot, &user_actions](TgBot::Message::Ptr message)
                              {
        user_actions[message->chat->id] = "video";
        bot.getApi().sendMessage(message->chat->id, "mode changed to video convert"); });

    bot.getEvents().onCommand("music", [&bot, &user_actions](TgBot::Message::Ptr message)
                              {
        user_actions[message->chat->id] = "music";
        bot.getApi().sendMessage(message->chat->id, "mode changed to music from video"); });

    bot.getEvents().onAnyMessage([&bot, &user_actions](TgBot::Message::Ptr message)
                                 {
        if (message->text.empty() || message->text[0] == '/') return;

        string action = user_actions[message->chat->id];
        string url = message->text;
        int64_t chat_id = message->chat->id;

        if (action == "video") {
            bot.getApi().sendMessage(chat_id, "wait a second");
            if (get_video_size(url) > 30000000) {
                bot.getApi().sendMessage(chat_id, "video is too big");
                return;
            }
            string output_file = "video_" + to_string(chat_id) + ".mp4";
            FileGuard guard(output_file);
            string command = "yt-dlp -f \"bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best\" "
                              "--merge-output-format mp4 --no-playlist -N 8 "
                              "\"" + url + "\" -o \"" + output_file + "\"";
            bot.getApi().sendMessage(chat_id, "converting...");
            system(command.c_str());

            if (!filesystem::exists(output_file)) {
                bot.getApi().sendMessage(chat_id, "не вдалось завантажити відео");
                return;
            }
            try {
                bot.getApi().sendMessage(chat_id, "sending video...");
                bot.getApi().sendVideo(chat_id, TgBot::InputFile::fromFile(output_file, "video/mp4"));
            } catch (const std::exception& e) {
                bot.getApi().sendMessage(chat_id, "Error. Telegram can't afford this size of video :(");
                if (filesystem::exists(output_file)) filesystem::remove(output_file);
            }
        }
        else if (action == "music") {
            bot.getApi().sendMessage(chat_id, "wait a second");
            if (get_video_size(url) > 50000000) {
                bot.getApi().sendMessage(chat_id, "too big sound");
                return;
            }
            string timestamp = to_string(chrono::steady_clock::now().time_since_epoch().count());
            string output_file = "audio_" + to_string(chat_id) + "_" + timestamp + ".mp3";
            string command = "yt-dlp --impersonate chrome --no-playlist "
                              "-x --audio-format mp3 \"" + url + "\" -o \"" + output_file + "\"";
            bot.getApi().sendMessage(chat_id, "converting...");
            system(command.c_str());

            if (!filesystem::exists(output_file)) {
                bot.getApi().sendMessage(chat_id, "не вдалось завантажити аудіо");
                return;
            }
            try {
                bot.getApi().sendMessage(chat_id, "sending audio...");
                bot.getApi().sendAudio(chat_id, TgBot::InputFile::fromFile(output_file, "audio/mpeg"));
            } catch (const std::exception& e) {
                bot.getApi().sendMessage(chat_id, "Error. Telegram can't afford this size of sound :(");
                if (filesystem::exists(output_file)) filesystem::remove(output_file);
            }
        } });

    httplib::Server svr;

    svr.Post("/webhook", [&bot](const httplib::Request &req, httplib::Response &res)
             {
    try {
        boost::property_tree::ptree pt;
        std::istringstream iss(req.body);
        boost::property_tree::read_json(iss, pt);

        TgBot::TgTypeParser typeParser;
        TgBot::Update::Ptr update = typeParser.parseJsonAndGetUpdate(pt);

        bot.getEventHandler().handleUpdate(update);
    } catch (const std::exception& e) {
        cerr << "Помилка обробки update: " << e.what() << endl;
    }
    res.set_content("OK", "text/plain"); });

    svr.Get("/", [](const httplib::Request &, httplib::Response &res)
            { res.set_content("Bot is running", "text/plain"); });

    cout << "Starting HTTP server on port " << port << endl;
    svr.listen("0.0.0.0", port);

    return 0;
}