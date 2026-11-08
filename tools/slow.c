/*
 * slow - a stdio proxy that slows an engine down so games can be watched.
 *
 * Search-free engines (random movers, one-ply analysers, neural nets)
 * answer instantly whatever the clock says, so a game in a GUI is over in
 * a flash. Our own engines have a MoveDelay option for this; slow.exe does
 * the same for third-party ones. It sits between the GUI and the engine,
 * passes every line through unchanged, and holds any line that announces
 * a move until at least N ms have passed since the GUI's last line (for UCI
 * that is the "go", for xboard the move it fed). Everything else (uciok,
 * readyok, feature, pong, info) passes at once.
 *
 * Configuration: a text file next to the proxy with the proxy's own base
 * name and .txt, e.g. slow.exe reads slow.txt:
 *
 *     fairy-random_x86-64.exe
 *     2000
 *
 * line 1 the engine command (relative to the proxy's folder), line 2 the
 * delay in ms, and an optional line 3 that replaces every "go ..." the GUI
 * sends, e.g. "go movetime 20" or "go nodes 1000000", which turns a normal
 * UCI engine into a fixed-effort reference whatever the tournament clock
 * says. Point the GUI at slow.exe with the engine's own protocol.
 *
 * Build:  x86_64-w64-mingw32-gcc -O2 -static -o slow.exe slow.c -lpthread
 *         gcc -O2 -o slow slow.c -lpthread
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
static long long now_ms(void) { return (long long)GetTickCount64(); }
static void sleep_ms(int ms) { Sleep(ms); }
#else
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <libgen.h>
static long long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000LL + t.tv_nsec / 1000000; }
static void sleep_ms(int ms) { struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L }; nanosleep(&ts, NULL); }
#endif

static int delay_ms = 1000;
static char go_override[256];             /* line 3 of the config, if any */
static volatile long long last_in;        /* time of the GUI's last line */
static volatile int gui_closed;

#ifdef _WIN32
static HANDLE child_in, child_out, child_proc, our_out;
static void write_out(const char *s) { DWORD w; WriteFile(our_out, s, (DWORD)strlen(s), &w, NULL); }
static void write_child(const char *s) { DWORD w; WriteFile(child_in, s, (DWORD)strlen(s), &w, NULL); }
static int read_child(char *buf, int max) { DWORD r; if (!ReadFile(child_out, buf, (DWORD)max, &r, NULL) || r == 0) return 0; return (int)r; }
#else
static int child_in, child_out; static pid_t child_pid;
static void write_out(const char *s) { ssize_t r = write(1, s, strlen(s)); (void)r; }
static void write_child(const char *s) { ssize_t r = write(child_in, s, strlen(s)); (void)r; }
static int read_child(char *buf, int max) { ssize_t r = read(child_out, buf, max); return r <= 0 ? 0 : (int)r; }
#endif

/* does this engine line announce a move? UCI bestmove, xboard "move e2e4",
   and the protocol-1 forms "My move is: e2e4" and "1. ... e2e4" */
static int is_move_line(const char *s) {
  if (!strncmp(s, "bestmove", 8)) return 1;
  if (!strncmp(s, "move ", 5)) return 1;
  if (!strncmp(s, "My move is", 10)) return 1;
  if (isdigit((unsigned char)s[0])) { while (isdigit((unsigned char)*s)) s++; if (s[0] == '.' && s[1] == ' ') return 1; }
  return 0;
}

/* GUI -> engine, every line forwarded at once and time-stamped */
static void *gui_thread(void *arg) {
  char line[4096];
  (void)arg;
  while (fgets(line, sizeof line, stdin)) {
    last_in = now_ms();
    if (go_override[0] && !strncmp(line, "go", 2) && (line[2] == '\n' || line[2] == ' ' || line[2] == '\r')) {
      write_child(go_override); write_child("\n");
    } else write_child(line);
  }
  gui_closed = 1;
  return NULL;
}

static int start_child(const char *cmd) {
#ifdef _WIN32
  SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
  HANDLE cin_r, cin_w, cout_r, cout_w;
  STARTUPINFOA si; PROCESS_INFORMATION pi; char cl[2048];
  if (!CreatePipe(&cin_r, &cin_w, &sa, 0) || !CreatePipe(&cout_r, &cout_w, &sa, 0)) return 0;
  SetHandleInformation(cin_w, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(cout_r, HANDLE_FLAG_INHERIT, 0);
  memset(&si, 0, sizeof si); si.cb = sizeof si;
  si.dwFlags = STARTF_USESTDHANDLES; si.hStdInput = cin_r; si.hStdOutput = cout_w; si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  snprintf(cl, sizeof cl, "%s", cmd);
  if (!CreateProcessA(NULL, cl, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) return 0;
  CloseHandle(cin_r); CloseHandle(cout_w); CloseHandle(pi.hThread);
  child_out = cout_r; child_in = cin_w; child_proc = pi.hProcess;
#else
  int a[2], b[2];
  if (pipe(a) || pipe(b)) return 0;
  child_pid = fork();
  if (child_pid < 0) return 0;
  if (child_pid == 0) {
    char *argv[64], *tok, buf[2048]; int n = 0;
    snprintf(buf, sizeof buf, "%s", cmd);
    for (tok = strtok(buf, " "); tok && n < 63; tok = strtok(NULL, " ")) argv[n++] = tok;
    argv[n] = NULL;
    dup2(a[0], 0); dup2(b[1], 1); close(a[0]); close(a[1]); close(b[0]); close(b[1]);
    execv(argv[0], argv); _exit(127);
  }
  close(a[0]); close(b[1]); child_in = a[1]; child_out = b[0];
#endif
  return 1;
}

int main(int argc, char **argv) {
  char self[2048], cfgpath[2200], cmd[2048] = "", line[4096], buf[4096];
  char *dot; FILE *f; pthread_t th; int blen = 0, bpos = 0, n = 0;
  (void)argc; (void)argv;
#ifdef _WIN32
  GetModuleFileNameA(NULL, self, sizeof self);
  our_out = GetStdHandle(STD_OUTPUT_HANDLE);
  _setmode(_fileno(stdout), _O_BINARY);
#else
  snprintf(self, sizeof self, "%s", argv[0]);
#endif
  /* work in the proxy's own folder so the engine command and its files resolve */
  {
    char dir[2048]; char *slash;
    snprintf(dir, sizeof dir, "%s", self);
    slash = strrchr(dir, '/');
#ifdef _WIN32
    { char *bs = strrchr(dir, '\\'); if (!slash || (bs && bs > slash)) slash = bs; }
    if (slash) { *slash = 0; SetCurrentDirectoryA(dir); }
#else
    if (slash) { *slash = 0; if (chdir(dir)) { /* keep going */ } }
#endif
  }
  snprintf(cfgpath, sizeof cfgpath, "%s", self);
  dot = strrchr(cfgpath, '.');
  if (dot && !strchr(dot, '/') && !strchr(dot, '\\')) *dot = 0;
  strcat(cfgpath, ".txt");
  f = fopen(cfgpath, "r");
  if (!f) { fprintf(stderr, "slow: cannot open %s\n", cfgpath); return 1; }
  if (fgets(cmd, sizeof cmd, f)) cmd[strcspn(cmd, "\r\n")] = 0;
  if (fgets(line, sizeof line, f)) delay_ms = atoi(line);
  if (fgets(line, sizeof line, f)) { line[strcspn(line, "\r\n")] = 0; if (!strncmp(line, "go", 2)) snprintf(go_override, sizeof go_override, "%.255s", line); }
  fclose(f);
  if (!cmd[0]) { fprintf(stderr, "slow: %s has no engine command\n", cfgpath); return 1; }
  if (!start_child(cmd)) { fprintf(stderr, "slow: cannot start %s\n", cmd); return 1; }

  last_in = now_ms();
  pthread_create(&th, NULL, gui_thread, NULL);

  /* engine -> GUI, move lines held back */
  for (;;) {
    char c;
    if (bpos >= blen) { blen = read_child(buf, sizeof buf); bpos = 0; if (blen <= 0) break; }
    c = buf[bpos++];
    if (c == '\r') continue;
    if (c != '\n') { if (n < (int)sizeof line - 3) line[n++] = c; continue; }
    line[n] = 0;
    if (is_move_line(line)) {
      long long due = last_in + delay_ms;
      while (now_ms() < due && !gui_closed) sleep_ms(5);
    }
    line[n++] = '\n'; line[n] = 0;
    write_out(line);
    n = 0;
  }
  /* the GUI thread may still be blocked in fgets holding stdin's lock, and a
     normal exit would wait for it while flushing streams: leave directly */
#ifdef _WIN32
  WaitForSingleObject(child_proc, 2000); TerminateProcess(child_proc, 0);
  ExitProcess(0);
#else
  { int st; if (waitpid(child_pid, &st, WNOHANG) == 0) { sleep_ms(200); kill(child_pid, SIGTERM); waitpid(child_pid, &st, 0); } }
  _exit(0);
#endif
}
