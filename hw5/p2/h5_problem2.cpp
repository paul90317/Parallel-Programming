#include <cstdlib>
#include <iostream>
#include <cstring>
#include <unordered_map>
#include <fstream>
#include <string>
#include <vector>
#include <omp.h>
#include <queue>
#include <sys/time.h>

using namespace std;

#define get_id omp_get_thread_num()
#define BUF_SIZE 1000

unordered_map<string, int> keywords;
vector<string> infiles;
int p_finish = 0;

class node
{
public:
    char *s;
    node *next;
};
node *new_node(char *s)
{
    node *t = (node *)malloc(sizeof(node));
    t->s = s;
    t->next = NULL;
    return t;
}
class thread_safe_queue
{
private:
    node *start, *end;

public:
    thread_safe_queue()
    {
        start = NULL,
        end = NULL;
    }
    void push(char *buf)
    {
        char *s = (char *)malloc(strlen(buf) + 1);
        memcpy(s, buf, strlen(buf) + 1);
        if (end == NULL)
        {
            end = new_node(s);
            start = end;
            return;
        }
        end->next = new_node(s);
        end = end->next;
    }
    char *pop()
    {
        if (start == NULL)
            return NULL; // make it safe
        node *t = start;
        char *s = t->s;
        start = start->next;
        free(t);
        if (start == NULL)
            end = NULL;
        return s;
    }
    bool size() { return start; }
    bool empty() { return !end; }
};

thread_safe_queue shared_q;

string token(char **p)
{
    int i = 0;
    char *old = *p;
    char *s = *p;
    while (s[i] && s[i] != ' ')
        i++; //有效字元
    if (s[i] == 0)
    {
        *p = s + i;
        return old;
    }
    s[i++] = 0; //切割字串
    while (s[i] && s[i] == ' ')
        i++; //空白
    *p = s + i;
    return old;
}

void Consumer(char *ss)
{ // for buf's pointer
    string s;
    while (s = token(&ss), s.size())
    {
        auto k = keywords.find(s);
        if (k != keywords.end())
        {
#pragma omp critical
            k->second++;
        }
    }
}
void Producer(string infile)
{
    fstream fp;
    char buf[BUF_SIZE];
    fp.open(infile, ios::in);
    while (fp.getline(buf, sizeof(buf)))
    {
#pragma omp critical
        shared_q.push(buf);
    }
    fp.close();
}
int main(int argc, char *argv[])
{
    char *dir = "res";
    char *skeyword = "keywords.txt";
    char *tmpfname = "0.tmp.txt";
    int np = 0, nc = 4;

    // argv
    for (int i = 0; i < argc; i++)
    {
        if (argv[i][0] == '-' && i + 1 < argc)
        {
            switch (argv[i][1])
            {
            case 'f': // res 位置
                dir = argv[++i];
                break;
            case 'p': //#producer
                np = strtol(argv[++i], NULL, 10);
                break;
            case 'c': //#consumer
                nc = strtol(argv[++i], NULL, 10);
                break;
            case 'k': // keywords 檔名
                skeyword = argv[++i];
                break;
            }
        }
    }

    char buf[BUF_SIZE];
    fstream fp;

    //取得 infiles
    sprintf(buf, "ls %s/*.txt > %s", dir, tmpfname);
    system(buf);
    fp.open(tmpfname, ios::in);
    while (fp.getline(buf, sizeof(buf)))
    {
        infiles.push_back(buf);
    }
    fp.close();

    //取得 keywords
    string s;
    fp.open(skeyword, ios::in);
    do
    {
        fp >> s;
        keywords[s] = 0;
    } while (!fp.eof());
    fp.close();

    //若沒指定 np，根據檔案數量分配
    if (np <= 0)
    {
        np = infiles.size();
    }

    //計時
    struct timeval begin, end;
    gettimeofday(&begin, 0);

    //平行計算區
#pragma omp parallel num_threads(np + nc)
    {
        int id = get_id;
        if (id < np)
        {
            // Producer code
            for (int i = id; i < infiles.size(); i += np)
            {
                Producer(infiles[i]);
            }
#pragma omp atomic
            p_finish++;
        }
        else
        {
            // Consumer code
            char *s;
            while (p_finish < np || shared_q.size())
            {
                if (shared_q.empty())
                    continue;
#pragma omp critical
                s = shared_q.pop();
                if (s)
                {
                    Consumer(s);
                    free(s);
                }
            }
        }
    }

    //結束計時
    gettimeofday(&end, 0);
    long seconds = end.tv_sec - begin.tv_sec;
    long microseconds = end.tv_usec - begin.tv_usec;
    double elapsed = seconds + microseconds * 1e-6;

    fp.open(skeyword, ios::in);
    do
    {
        fp >> s;
        cout << s << " " << keywords[s] << "\n";
    } while (!fp.eof());
    fp.close();

    cout << "\n";
    cout << "number of producers: " << np << "\n";
    cout << "number of consumers: " << nc << "\n";
    cout << "The execution time: " << elapsed << "\n";
}