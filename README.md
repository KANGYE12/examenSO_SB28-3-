# 🐚 Linux Job Control Shell

A lightweight Linux shell with built-in support for job control, background/foreground execution, signal handling, redirections, and command parsing.

> Developed as part of the **Operating Systems** course at the **University of Málaga (UMA)**, based on material from *Operating System Concepts Essentials* by Silberschatz et al.

---

## 📁 Project Structure

- `shell.c`: Main shell loop with process management, job control, and command execution.
- `job_control.c`: Module for handling jobs (linked list, states, signal handling).
- `job_control.h`: Header file with job data structures, macros, and function declarations.

---

## 🚀 Features

- Foreground (`fg`) and background (`bg`) process handling
- Built-in commands: `cd`, `jobs`, `fg`, `bg`, `exit`
- Input/output redirection: `< input.txt`, `> output.txt`
- Job list management (start, stop, resume, terminate)
- Signal handling for:
  - `SIGCHLD`: Detect background job termination
  - `SIGHUP`: Custom behavior (`hup.txt` logging)
- Comment parsing (`#` with escape support)
- Command line parsing with whitespace and redirection handling

---

## 🛠️ Compilation

Make sure you have `gcc` installed. To compile the project:

```bash
gcc shell.c job_control.c -o shell
