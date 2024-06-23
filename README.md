# FP Sisop 
### Cara Jalanin
1. Compile `discorit.c` dan `server.c` dengan cara
   ```bash
   gcc -o server server.c -lcrypt
   gcc -o discorit discorit.c
   ```
2. Buat tree seperti berikut
   ```bash
      ├── DiscoritIT
      │   ├── channels.csv
      │   └── users.csv
      ├── discorit
      ├── discorit.c
      ├── server
      └── server.c  
   ```

3. Untuk fitur sudah bisa Login dan Register, lalu tercatat pada users.csv nya.
