#include <alsa/asoundlib.h>
#include <spawn.h>
#include "Microphone.hpp"

#include "extension/Configuration.hpp"
#include "json.h"

namespace module::input {

using extension::Configuration;


Voice2jsonParsedIntent parse_voice2json_json(char *json_text, size_t size) {
    Voice2jsonParsedIntent intent = {};
    
    json_value_s *root = json_parse(json_text, size);
    
    json_object_s *root_object = (json_object_s*) root->payload;
    json_object_element_s *root_element = root_object->start;
    while(root_element) {
        json_string_s *element_name = root_element->name;
                
        if(strcmp(element_name->string, "text") == 0) {
            json_string_s *text_str = json_value_as_string(root_element->value);
            intent.text = strdup(text_str->string);
        } else if(strcmp(element_name->string, "intent") == 0) {
            json_object_s *intent_obj = (json_object_s*) root_element->value->payload;
            json_object_element_s *intent_element = intent_obj->start;
            
            assert(strcmp(intent_element->name->string, "name") == 0);
            json_string_s *intent_name_str = json_value_as_string(intent_element->value);
            intent.intent = strdup(intent_name_str->string);
            
            intent_element = intent_element->next;
            
            assert(strcmp(intent_element->name->string, "confidence") == 0);
            json_number_s *confidence_num = json_value_as_number(intent_element->value);
            intent.confidence = atof(confidence_num->number);
        } else if(strcmp(element_name->string, "slots") == 0) {
            //TODO: need to do parsing of slots...
            
        }
        
        root_element = root_element->next;
    }
    free(root);
    
    return intent;
}

//TODO: there no way of testing this for the moment.
//Use this reference example with "snd_pcm_poll_descriptors"
//Which returns a file handle we can wait on.
//https://gist.github.com/albanpeignier/104902
/*
bool start_microphone_capture() {
    const char *device_name = "default";

    int err = 0;
    snd_pcm_t *capture_handle;
    err = snd_pcm_open(&capture_handle, device_name, SND_PCM_STREAM_CAPTURE, 0);
    if(err < 0) {
        return false
    }
    
    
    
    return true;
}
*/

//handles: out parameter containing the stdin, stdout, stderr handles from the
//spawned process
//first char * in args array refers to the process name
bool spawn_process(MicProcHandles *handles, char *const *args) {
    bool success = false;
    
    pid_t pid = 0;
    int stdout_pipe[2] = {};
    int stderr_pipe[2] = {};
    int stdin_pipe[2] = {};
    posix_spawn_file_actions_t actions = {};
    
    char *path = getenv("PATH");
    assert(path);
    
    size_t len = strlen(path) + 5 + 1;
    char *env_path = (char*) malloc(len);
    int written = sprintf(env_path, "PATH=%s", path);
    assert(written > 0);
    env_path[written] = 0;
    
    char *const envp[] = { 
        (char*) "KALDI_DIR=", //KALDI_DIR env var must be blank!
        (char*) env_path, 
        NULL 
    };
    
    if((pipe(stdout_pipe) == -1) || (pipe(stderr_pipe) == -1) || (pipe(stdin_pipe) == -1)) {
        goto cleanup;
    }
    
    posix_spawn_file_actions_init(&actions);
    
    posix_spawn_file_actions_addclose(&actions, stdout_pipe[0]);
    posix_spawn_file_actions_addclose(&actions, stderr_pipe[0]);
    posix_spawn_file_actions_addclose(&actions, stdin_pipe[1]);

    posix_spawn_file_actions_adddup2(&actions, stdout_pipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, stderr_pipe[1], STDERR_FILENO);
    posix_spawn_file_actions_adddup2(&actions, stdin_pipe[0], STDIN_FILENO);
    
    posix_spawn_file_actions_addclose(&actions, stdout_pipe[1]);
    posix_spawn_file_actions_addclose(&actions, stderr_pipe[1]);
    posix_spawn_file_actions_addclose(&actions, stdin_pipe[0]);
        
    if(posix_spawnp(&pid, args[0], &actions, 0, args, envp) != 0) {
        goto cleanup;
    }
    
    success = true;
cleanup:
    if(stdout_pipe[1]) close(stdout_pipe[1]);
    if(stderr_pipe[1]) close(stderr_pipe[1]);
    if(stdin_pipe[0]) close(stdin_pipe[0]);

    *handles = {stdout_pipe[0], stderr_pipe[0], stdin_pipe[1]};
    
    if(actions.__actions) {
        posix_spawn_file_actions_destroy(&actions);
    }
    if(env_path) {
        free(env_path);
    }
    
    return success;
}

//python -m voice2json --base-directory /home/nubots/voice2json/ -p en transcribe-wav
bool voice2json_transcribe_stream(MicProcHandles *handles) {
    char *const args[] = {
        (char*) "python",     
        (char*) "-m",
        (char*) "voice2json",
        (char*) "--base-directory",
        (char*) "/home/nubots/voice2json/",
        (char*) "-p",
        (char*) "en",
        (char*) "transcribe-stream",
        NULL
    };
    
    return spawn_process(handles, args);
}

//python -m voice2json --base-directory /home/nubots/voice2json/ -p en transcribe-wav --input-size
bool voice2json_transcribe_wav(MicProcHandles *handles) {
    char *const args[] = {
        (char*) "python",     
        (char*) "-m",
        (char*) "voice2json",
        (char*) "--base-directory",
        (char*) "/home/nubots/voice2json/",
        (char*) "-p",
        (char*) "en",
        (char*) "transcribe-wav",
        (char*) "--input-size",
        NULL
    };
    
    return spawn_process(handles, args);
}

char *voice2json_recognize_intent(char *input) {
    char *output = 0;
    char buffer[0x1000];
    ssize_t bytes_read = 0;
    
    char *const args[] = {
        (char*) "python",     
        (char*) "-m",
        (char*) "voice2json",
        (char*) "--base-directory",
        (char*) "/home/nubots/voice2json/",
        (char*) "-p",
        (char*) "en",
        (char*) "recognize-intent",
        NULL
    };
    
    MicProcHandles handles = {};
    if(!spawn_process(&handles, args)) {
        goto cleanup;
    }
    write(handles.stdin, input, strlen(input));
    
    bytes_read = read(handles.stdout, buffer, sizeof(buffer)-1);
    assert(bytes_read > 0);
    buffer[bytes_read] = 0;
    
    output = strdup(buffer);
    
cleanup:;
    if(handles.stdout) close(handles.stdout);
    if(handles.stderr) close(handles.stderr);
    if(handles.stdin) close(handles.stdin);

    return output;
}

void write_audio_to_file(int fd, char *filename) {
    FILE *file = fopen(filename, "rb");
    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    assert(file_size != -1);
    
    char *file_buffer = (char*) malloc(file_size);
    fread(file_buffer, file_size, 1, file);
    fclose(file);
    
    char file_size_buf[64];
    sprintf(file_size_buf, "%d\n", file_size);
        
    write(fd, file_size_buf, strlen(file_size_buf));
    
    ssize_t write_res = write(fd, file_buffer, file_size);
    assert(write_res == file_size);

    free(file_buffer);
}


Microphone::Microphone(std::unique_ptr<NUClear::Environment> environment) : Reactor(std::move(environment)), config{} {
    
    on<Configuration>("Microphone.yaml").then([this](const Configuration& cfg) {
        // Use configuration here from file Microphone.yaml
        this->log_level = cfg["log_level"].as<NUClear::LogLevel>();
    });
    

    
    if(!voice2json_transcribe_wav(&handles)) {
        perror("failed to start voice2json");
        return;
    }
    
    on<Trigger<MicControlMsg>>().then([this](const MicControlMsg& msg) {
        switch(msg.type) {
            case MIC_MSG_ENABLE: 
                enabled = true;
            break;
            case MIC_MSG_DISABLE:
                enabled = false;
            break;
            case MIC_MSG_TEST_AUDIO:
                char filename[1024];
                sprintf(filename, "/home/nubots/nuhear/audio/voice_%03d.wav", msg.data);
            
                write_audio_to_file(handles.stdin, filename);
            break;
        }
    });
    
    
    on<IO>(handles.stdout, IO::READ).then([this] {
        printf("fds[0].revents & POLLIN\n");
            
        //TODO: need to read and append to buffer until we reach a newline...
        char buffer[0x1000];
        ssize_t bytes_read = read(handles.stdout, buffer, sizeof(buffer)-1);
        if(bytes_read > 0) {
            assert((size_t)bytes_read < sizeof(buffer)); //TODO
            buffer[bytes_read] = 0;
                        
            if(enabled) {            
                char *intent_json = voice2json_recognize_intent(buffer);
                if(!intent_json) {
                    perror("error");
                }
                printf("%s\n", intent_json);
                
                Voice2jsonParsedIntent intent = parse_voice2json_json(intent_json, strlen(intent_json));
                //TODO: emit this as a message
                printf("intent = %s\ntext = %s\ncondifence = %f\n", intent.intent, intent.text, intent.confidence);
                
                
/*              

                parse_voice2json_json(intent_json, strlen(intent_json));
                std::unique_ptr<MicSpeechIntent> msg = std::make_unique<MicSpeechIntent>(MicSpeechIntent{
                    123, 2
                        
                });
                emit(std::move(msg));
*/    

                free(intent_json);
            }
            
        }
    });
    
    //TODO: have a better story on how we handle this
    //for the moment we fatally error
    on<IO>(handles.stderr, IO::READ).then([this] {
       printf("fds[1].revents & POLLIN\n");
            
        char buffer[0x1000];
        ssize_t bytes_read = read(handles.stderr, buffer, sizeof(buffer)-1);
        if(bytes_read > 0) {
            assert((size_t)bytes_read < sizeof(buffer)); //TODO
            buffer[bytes_read] = 0;
            
            printf("STDERR (%ld) = %s\n", bytes_read, buffer);
            
            perror("stderr");
        } 
    });

#if 1
    //this is just for testing
    //we need this as we do not have an
    //environment for testing the microphone
    on<IO>(STDIN_FILENO, IO::READ).then([this] {
        char b = 0;
        ssize_t bytes_read = read(STDIN_FILENO, &b, 1);
        assert(bytes_read == 1);
        switch(b) {
            case 'q':
                char *string = (char*)malloc(100);
                size_t size = 100;
                bytes_read = getline(&string, &size, stdin);
                
                uint32_t num = atoi(string);
                printf("%ld,%d - %s \n", bytes_read, num, string);
                
                std::unique_ptr<MicControlMsg> msg = std::make_unique<MicControlMsg>(MicControlMsg{
                    MIC_MSG_TEST_AUDIO, num
                    
                });
                emit(std::move(msg));
                
                free(string);
            break;
        }        
    });
#endif
    
}

Microphone::~Microphone() {
    if(handles.stdout) close(handles.stdout);
    if(handles.stderr) close(handles.stderr);
    if(handles.stdin) close(handles.stdin);
}


}  // namespace module::input
