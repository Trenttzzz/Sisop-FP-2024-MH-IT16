# Laporan Praktikum FP Sistem Operasi IT16

## Pre-Requisite
1. Compile `discorit.c`, `server.c`, dan `monitor.c` dengan cara
   ```bash
   gcc -o server server.c -lcrypt
   gcc -o discorit discorit.c
   gcc -o monitor monitor.c
   ```
2. Buat tree seperti berikut
   ```bash
      ├── DiscoritIT
      │   ├── channels.csv
      │   └── users.csv
      ├── discorit.c
      ├── monitor.c
      └── server.c  
   ```
3. jalankan program nya dengan cara:
   - Buka terminal
   - Change Directory ke Directory yang ada file nya
   - jalanankan perintah berikut
      ```bash
      # jalankan server
      ./server

      # jalankan discorit
      ./discorit <isi dari command>
      ```
   - Terakhir bisa open new terminal, lalu jalankan `monitor.c`
      ```bash
      ./monitor
      ```

## Showcase Fitur - Fitur yang Ada

### A. Authentikasi
1. **Register / Login**

   Command : 
      ```bash
      # register user
      ./disccorit REGISTER <username> -p <password>
      
      # login user
      ./discorit LOGIN <username> -p <password>
      ```

   Contoh :
      ```bash
      # register user
      ./discorit REGISTER azza -p azza123

      # login user
      ./discorit LOGIN azza -p azza123
      ```

   Screenshot :

      ```bash
      zaa@Azza-TUF:~/Desktop/sisop/showcase$ ./discorit REGISTER azza -p azza123
      azza berhasil register23
      zaa@Azza-TUF:~/Desktop/sisop/showcase$ ./discorit LOGIN azza -p azza123
      User azza logged in
      azza berhasil login23
      [azza]
      ```
   
   Setelah menjalankan perintah diatas, maka username beserta password akan tercatat pada `users.csv` dengan format berikut
   ```csv
   id,username,hash password,role
   1,azza,$2b$12$XXXXXXXXXXXXXXXXXXXXXOCHoRfgpnCMhcucnvTUJhRu9ksCFoRdS,ROOT

   ```

### B. Bagaimana DiscorIT Digunakan
1. **List Channel dan Room**

   Setelah login user dapat melihat channel dan room yang tersedia

   berikut adalah ketika list channel:

   ```bash
   [azza] LIST CHANNEL
   Channels: test coba
   [azza]
   ```

   List room:

   ```bash
   [azza/coba] LIST ROOM
   Rooms: chatme chatwho?
   [azza/coba]
   ```

2. **Akses Channel dan Room**
   
   Admin channel dan room bisa langsung masuk tanpa memasukkan password
   
   ```bash
   [azza] LIST CHANNEL
   Channels: test coba
   [azza] JOIN coba
   [azza/coba]
   ```

   ketika user ingin masuk kedalam channel harus memasukkan *key* terlebih dahulu

   ```bash
   [userbiasa] LIST CHANNEL
   Channels: test coba
   [userbiasa] JOIN coba
   Key: coba123
   [userbiasa/coba]
   ```

   setelah join channel user bisa langsung masuk kedalam room yang diinginkan

   ```bash
   [userbiasa/coba] LIST ROOM
   Rooms: chatme chatwho?
   [userbiasa/coba] JOIN chatme
   [userbiasa/coba/chatme]
   ```

3. **Fitur Chat**
   Setiap user dapat mengirim pesan dalam chat. ID pesan chat dimulai dari 1 dan semua pesan disimpan dalam file `chat.csv`. User dapat **melihat** pesan-pesan chat yang ada dalam room. Serta user dapat **edit dan delete** pesan yang sudah dikirim dengan menggunakan ID pesan.

   berikut adalah cara menjalankannya

   - **Buat chat dan lihat chat**

      ```bash
      [userbiasa/coba/chatme] CHAT "hai"
      Chat Baru: hai
      [userbiasa/coba/chatme] CHAT "halo juga"
      Chat Baru: halo juga
      [userbiasa/coba/chatme] SEE CHAT
      Chat:
      [2024-06-28 16:23:22][1][userbiasa] "hai"
      [2024-06-28 16:24:05][2][userbiasa] "halo juga"

      ```

   - **Delete chat**

      ```bash
      [qurbancare/care/urban] DEL CHAT 3
      [qurbancare/care/urban] CHAT DELETED
      ```

   - **Edit Chat**

      ```bash
      [qurbancare/care/urban] CHAT “hallo”
      [qurbancare/care/urban] SEE CHAT
      sebelumnya
      [qurbancare/care/urban] EDIT CHAT 3 “hi”
      [05/06/2024 23:22:12][3][qurbancare] “hi”
      sesudahnya
      ```

### C. Root

- Akun yang pertama kali mendaftar otomatis mendapatkan peran "root".

   seperti pada contoh `users.csv` berikut
   ```bash
   1,azza,$2b$12$XXXXXXXXXXXXXXXXXXXXXOCHoRfgpnCMhcucnvTUJhRu9ksCFoRdS,ROOT
   2,userbiasa,$2b$12$XXXXXXXXXXXXXXXXXXXXXOM/rp0Rg14xFzslgLSLhOXSvDlo2rJl2,USER

   ```
   pada kolom terakhir user pertama pasti **ROOT** dan setelahnya menjadi **USER** by default

- Root dapat masuk ke channel manapun tanpa key dan create, update, dan delete pada channel dan room.
   pada contoh kali ini user "azza" adalah root

   ```bash
   [azza] LIST CHANNEL
   Channels: test coba
   [azza] JOIN test
   [azza/test] CREATE ROOM coba2
   Room coba2 created
   [azza/test] LIST ROOM
   Rooms: coba2
   [azza/test] DEL ROOM coba2
   Room coba2 deleted
   [azza/test] LIST ROOM
   Rooms:
   [azza/test]
   ```

   *tambahkan edit channel

- Root memiliki kemampuan khusus untuk mengelola user, seperti: list, edit, dan Remove.

   ```
   [azza] LIST USER
   Users: [1]azza [2]userbiasa
   ```

   *tambahkan screenshot or copy terminal dari edit, dan remove

### D. Admin Channel
- Setiap user yang membuat channel otomatis menjadi admin di channel tersebut. Informasi tentang user disimpan dalam file `auth.csv`.
beirkut adalah isi dari `auth.csv`
   ```
   1,azza,ROOT
   2,userbiasa,USER

   ```
- Admin dapat create, update, dan delete pada channel dan room, serta dapat remove, ban, dan unban user di channel mereka

1. **Channel**

   Informasi tentang semua channel disimpan dalam file `channel.csv`. Semua perubahan dan aktivitas user pada channel dicatat dalam file `users.log`.

   - **channels.csv**
      ```
      1,coba,$2b$12$XXXXXXXXXXXXXXXXXXXXXOZekr6ez3qUe7L/XYHlw9JMx2CRvbn/O
      2,test,$2b$12$XXXXXXXXXXXXXXXXXXXXXOCPI2U2u7wpIutjhuq7R.IL8q0.czH/6

      ```

   - **users.log**
      ```
      [28/06/2024 15:56:54] azza buat coba
      [28/06/2024 16:02:58] azza masuk ke channel coba
      [28/06/2024 16:04:00] azza membuat room chatme
      [28/06/2024 16:04:16] azza membuat room chatwho?
      [28/06/2024 16:12:07] azza keluar dari channel coba
      [28/06/2024 16:14:39] userbiasa masuk ke channel coba
      [28/06/2024 16:15:22] userbiasa keluar dari channel coba
      [28/06/2024 16:15:49] azza masuk ke channel coba
      [28/06/2024 16:16:07] azza keluar dari channel coba
      [28/06/2024 16:16:19] userbiasa masuk ke channel coba
      [28/06/2024 16:16:24] userbiasa masuk ke room chatme
      [28/06/2024 16:16:38] userbiasa keluar dari room chatme
      [28/06/2024 16:16:49] userbiasa masuk ke room chatme
      [28/06/2024 16:29:08] userbiasa keluar dari room chatme
      [28/06/2024 16:29:10] userbiasa keluar dari channel coba
      [28/06/2024 16:29:21] azza masuk ke channel coba
      [28/06/2024 16:29:27] azza masuk ke room chatme
      [28/06/2024 16:47:42] azza keluar dari room chatme
      [28/06/2024 16:47:44] azza keluar dari channel coba

      ```
   
   berikut adalah cara untuk create channel pada programnya
   ```
   [azza] LIST CHANNEL
   Channels:
   [azza] CREATE CHANNEL coba -k coba123
   Channel coba dibuat
   [azza] LIST CHANNEL
   Channels: coba
   [azza] CREATE CHANNEL test -k test123
   Channel test dibuat
   [azza] LIST CHANNEL
   Channels: test coba
   ```

2. **Room**
   - Contoh penggunaan room
      ```
      [azza/coba] CREATE ROOM chatme
      Room chatme created
      [azza/coba] CREATE ROOM chatwho?
      Room chatwho? created
      [azza/coba] LIST ROOM
      Rooms: chatme chatwho?
      ```

3. **Ban**
   Admin dapat melakukan ban pada user yang nakal. Aktivitas ban tercatat pada `users.log`. Ketika di ban, role "user" berubah menjadi "banned". Data tetap tersimpan dan user tidak dapat masuk ke dalam channel.

   - Contoh:

      ```bash
      [qurbancare/care] BAN pen
      pen diban
      ```

4. **Unban**
   Admin dapat melakukan unban pada user yang sudah berperilaku baik. Aktivitas unban tercatat pada `users.log`. Ketika di unban, role "banned" berubah kembali menjadi "user" dan dapat masuk ke dalam channel.

   - Contoh: 

      ```bash
      [qurbancare/care] UNBAN pen
      pen kembali
      ```

5. **Remove User**
   Admin dapat remove user dan tercatat pada `users.log`.

   - Contoh: 

      ```
      [qurbancare/care] REMOVE USER pen
      pen dikick
      ```

### E. User
User dapat mengubah informasi profil mereka, user yang di ban tidak dapat masuk kedalam channel dan dapat keluar dari room, channel, atau keluar sepenuhnya dari DiscorIT.

1. Edit username

   - Contoh : 

      ```bash
      [qurbancare] EDIT PROFILE SELF -u pen
      Profil diupdate
      [pen]
      ```
   
2. Edit user password

   - Contoh : 

      ```bash
      [qurbancare] EDIT PROFILE SELF -p pen123
      Password diupdate
      ```

3. Banned user
   
   akan menampilkan pesan "anda telah di ban"

   - Contoh : 

      ```bash
      [qurbancare] JOIN care
      anda telah diban, silahkan menghubungi admin
      ```

4. Exit

   - Jika user lagi didalam room maka perintah EXIT akan mengakibatkan user keluar dari room tersebut
   - Jika user lagi didalam channel maka perintah EXIT akan mengakibatkan user keluar dari channel tersebut
   - Jika user tidak lagi didalam channel dan room maka program akan exit.

   - Contoh :

      ```
      [userbiasa/coba/chatme] EXIT
      Keluar Room
      [userbiasa/coba] EXIT
      Keluar Channel
      [userbiasa] EXIT
      zaa@Azza-TUF:~/Desktop/sisop/showcase$
      ```

### F. Error Handling
Jika ada command yang tidak sesuai penggunaannya. Maka akan mengeluarkan pesan error dan tanpa keluar dari program client.

- Contoh : 

   ```
   [farand] tes
   Perintah tidak valid
   [farand] sd
   Perintah tidak valid
   [farand] d
   Perintah tidak valid
   [farand] sdsda
   Perintah tidak valid
   [farand]
   ```

   dapat dilihat bahwa program mengeluarkan output berupa pesan dan tidak keluar dari programnya.

### G. Monitor
- User dapat menampilkan isi chat secara real-time menggunakan program monitor. Jika ada perubahan pada isi chat, perubahan tersebut akan langsung ditampilkan di terminal.
- Sebelum dapat menggunakan monitor, pengguna harus login terlebih dahulu dengan cara yang mirip seperti login di DiscorIT.
- Untuk keluar dari room dan menghentikan program monitor dengan perintah "EXIT".
- Monitor dapat digunakan untuk menampilkan semua chat pada room, mulai dari chat pertama hingga chat yang akan datang nantinya.


