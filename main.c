#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#define API_URL "https://api.groq.com/openai/v1/chat/completions"
#define API_KEY "GROQ_API_KEY"

#define RESPONSE_BUFFER_SIZE 8192
#define POST_DATA_SIZE 2048
#define TOPIC_BUFFER_SIZE 256

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total_size = size * nmemb;
    strncat((char *)userp, (char *)contents, total_size);
    return total_size;
}

CURL *initialize_curl(CURL *curl, const char *post_data, char *response)
{
    struct curl_slist *headers = NULL;

    curl = curl_easy_init();
    if (curl)
    {
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "Authorization: Bearer " API_KEY);

        curl_easy_setopt(curl, CURLOPT_URL, API_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    }
    return curl;
}

char *query_api(const char *topic)
{
    CURL *curl;
    CURLcode res;
    char *response = malloc(RESPONSE_BUFFER_SIZE);
    if (!response)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        return NULL;
    }
    response[0] = '\0';

    char post_data[POST_DATA_SIZE];
    snprintf(post_data, sizeof(post_data),
             "{\"messages\":[{\"role\":\"user\",\"content\":\"Provide a detailed explanation for the topic: %s. Include relevant information.\"}],\"model\":\"llama3-8b-8192\"}",
             topic);

    curl = initialize_curl(curl, post_data, response);
    if (curl)
    {
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(response);
            response = NULL;
        }
        curl_easy_cleanup(curl);
    }

    return response;
}

char *extract_content(const char *response)
{
    char *content = NULL;
    cJSON *json = cJSON_Parse(response);
    if (json)
    {
        cJSON *choices = cJSON_GetObjectItemCaseSensitive(json, "choices");
        if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0)
        {
            cJSON *choice = cJSON_GetArrayItem(choices, 0);
            cJSON *message = cJSON_GetObjectItemCaseSensitive(choice, "message");
            if (cJSON_IsObject(message))
            {
                cJSON *content_item = cJSON_GetObjectItemCaseSensitive(message, "content");
                if (cJSON_IsString(content_item))
                {
                    content = strdup(content_item->valuestring);
                }
            }
        }
        cJSON_Delete(json);
    }
    return content;
}

void generate_markdown(const char *topics_file)
{
    FILE *file = fopen(topics_file, "r");
    if (file == NULL)
    {
        perror("Failed to open topics file");
        return;
    }

    FILE *md_file = fopen("structured_notes.md", "w");
    if (md_file == NULL)
    {
        perror("Failed to open Markdown file");
        fclose(file);
        return;
    }

    fprintf(md_file, "# Study Notes\n\n");

    char topic[TOPIC_BUFFER_SIZE];
    while (fgets(topic, sizeof(topic), file))
    {
        topic[strcspn(topic, "\n")] = '\0';

        fprintf(md_file, "## %s\n\n", topic);

        char *response = query_api(topic);
        if (response)
        {
            char *content = extract_content(response);
            if (content)
            {
                fprintf(md_file, "%s\n\n", content);
                free(content);
            }
            free(response);
        }
    }

    fclose(file);
    fclose(md_file);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <topics_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    generate_markdown(argv[1]);
    return EXIT_SUCCESS;
}
