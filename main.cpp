#include <iostream>
#include <cstdlib> 
#include <array>
#include <boost/asio.hpp>
#include <filesystem>
#include <memory>
#include <cstdio>
#include <chrono>

namespace boost { namespace asio {
    using io_service = io_context;
}}

#include <tgbot/tgbot.h>

using namespace std;

long long get_video_size(string url){
    string command = "yt-dlp -f \"bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best\" "
                      "--print filesize_approx \"" + url + "\" 2>/dev/null";

    array<char, 128> buffer;
    string result;

    using PipeCloser = int(*)(FILE*);
    unique_ptr<FILE, PipeCloser> pipe(popen(command.c_str(), "r"), pclose);

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    if (result.empty()) return -1;

    try {
        return stoll(result);
    } catch (...) {
        return -1;
    }
}

int main() {
    const string token = "8899703710:AAHxo01VXivatHHseZ32ye3W4-XQv83-tdA";
    TgBot::Bot bot(token);
    
    string current_action = "";
    
    bot.getEvents().onCommand("start", [&bot](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "hello i am a bot URL tool who will work with your video");
    });

    bot.getEvents().onCommand("video", [&bot, &current_action](TgBot::Message::Ptr message) {
        current_action = "video";
        bot.getApi().sendMessage(message->chat->id, "mode changed to video convert");
    });

    bot.getEvents().onCommand("music", [&bot, &current_action](TgBot::Message::Ptr message) {
        current_action = "music";
        bot.getApi().sendMessage(message->chat->id, "mode changed to music from video");
    });
    
    bot.getEvents().onAnyMessage([&bot, &current_action](TgBot::Message::Ptr message){
        if (message->text.empty() || message->text[0] == '/') {
            return;
        }
        string url = message->text;
        if(current_action == "video"){ 
            current_action = "";
            bot.getApi().sendMessage(message->chat->id, "wait a second");
            if(get_video_size(url) > 50000000){
                bot.getApi().sendMessage(message->chat->id, "video is too big");
                return;
            }
            string output_file = "video_" + to_string(message->chat->id) + ".mp4";
            string command = "yt-dlp --extractor-args \"youtube:player_client=android,web\" "
                              "-f \"bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best\" "
                              "--merge-output-format mp4 \"" + url + "\" -o \"" + output_file + "\"";
            bot.getApi().sendMessage(message->chat->id, "converting...");
            int result = system(command.c_str());

            filesystem::path video_path = "output.mp4";
            try {
                bot.getApi().sendMessage(message->chat->id, "sending video...");

                bot.getApi().sendVideo(
                    message->chat->id,                              
                    TgBot::InputFile::fromFile(output_file, "video/mp4") 
                );
                filesystem::remove(output_file);
            } catch (const std::exception& e) {
                bot.getApi().sendMessage(message->chat->id, "error: " + std::string(e.what()));
            }
        }
        else if(current_action == "music"){
            current_action = "";
            bot.getApi().sendMessage(message->chat->id, "wait a second");
            
            string timestamp = to_string(chrono::steady_clock::now().time_since_epoch().count());
            string output_file = "audio_" + to_string(message->chat->id) + "_" + timestamp + ".mp3";
            string command = "yt-dlp -x --audio-format mp3 \"" + url + "\" -o \"" + output_file + "\"";
            
            if(get_video_size(url) > 50000000){
                bot.getApi().sendMessage(message->chat->id, "too big sound");
                return;
            }
            bot.getApi().sendMessage(message->chat->id, "converting...");
            int result = system(command.c_str());
            try {
                bot.getApi().sendMessage(message->chat->id, "sending audio...");
                bot.getApi().sendAudio(
                    message->chat->id,
                    TgBot::InputFile::fromFile(output_file, "audio/mpeg")
                );
                filesystem::remove(output_file);
            } catch (const std::exception& e) {
                bot.getApi().sendMessage(message->chat->id, "error: " + string(e.what()));
            }
        }
    });

    try {
        std::cout << "bot is working" << std::endl;
        TgBot::TgLongPoll longPoll(bot);
        bot.getApi().getUpdates(-1);
        while (true) {
            longPoll.start();
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
    }
    return 0;
}
