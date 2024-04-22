#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <stdbool.h>
#include <sys/wait.h>

#define PATH_MAX 4096
#define MAX_ENTRIES 100

struct FileMetadata {
    char entry_name[PATH_MAX];
    time_t last_modified;
    off_t size;
    mode_t permissions;
};

void update_snapshot(const char *dir_path, struct FileMetadata *old_snapshot) {
    printf("Updating snapshot for directory: %s\n", dir_path);

    FILE *snapshot = fopen("Snapshot.txt", "a"); // Open in write mode
    if (snapshot == NULL) {
        perror("Failed to open snapshot file");
        exit(EXIT_FAILURE);
    }

    fprintf(snapshot, "Snapshot of directory: %s\n", dir_path);
    fprintf(snapshot, "---------------------------------\n");

    DIR *dir;
    struct dirent *entry;
    dir = opendir(dir_path);
    if (dir == NULL) {
        perror("Error opening directory");
        fclose(snapshot);
        exit(EXIT_FAILURE);
    }

    struct FileMetadata new_snapshot[MAX_ENTRIES];
    int num_entries = 0;

    while ((entry = readdir(dir)) != NULL && num_entries < MAX_ENTRIES) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            struct stat st;
            char entry_path[PATH_MAX];
            snprintf(entry_path, PATH_MAX, "%s/%s", dir_path, entry->d_name);

            if (stat(entry_path, &st) == -1) {
                perror("Error getting file status");
                fclose(snapshot);
                closedir(dir);
                exit(EXIT_FAILURE);
            }

            struct FileMetadata metadata;
            strcpy(metadata.entry_name, entry->d_name);
            metadata.last_modified = st.st_mtime;
            metadata.size = st.st_size;
            metadata.permissions = st.st_mode;

            new_snapshot[num_entries++] = metadata;

            fprintf(snapshot, "Entry: %s\n", metadata.entry_name);
            fprintf(snapshot, "Size: %lld bytes\n", (long long)metadata.size);
            fprintf(snapshot, "Permissions: %o\n", metadata.permissions);
            fprintf(snapshot, "Last Modified: %s\n", ctime(&metadata.last_modified));
            fprintf(snapshot, "\n");

            // Recurse into subdirectories
            if (S_ISDIR(st.st_mode)) {
                // Pass the corresponding old_snapshot for the subdirectory
                update_snapshot(entry_path, old_snapshot);
            }
        }
    }

    closedir(dir);
    fclose(snapshot);

    exit(EXIT_SUCCESS); // Exit child process with success
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 12) {
        fprintf(stderr, "Usage: %s -o output_dir dir1 [dir2 ... dir10]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *output_dir = argv[1]; // Output directory
    struct FileMetadata old_snapshots[MAX_ENTRIES];

    // Initial update of snapshots
    for (int i = 2; i < argc; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) { // Child process
            // Execute update_snapshot for the directory
            update_snapshot(argv[i], NULL);
        }
    }

    // Wait for all child processes to finish
    int status;
    pid_t wpid;
    while ((wpid = wait(&status)) > 0) {
        printf("The process with PID %d has ended with code %d.\n", wpid, WEXITSTATUS(status));
    }

    return 0;
}

