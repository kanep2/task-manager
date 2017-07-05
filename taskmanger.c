/*
    CSCI 352 Assignment 3
    Process Monitor
    Kane Pollard, May 2016
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <gtk/gtk.h>
#include <linux/limits.h>

#define PIDMAX 32768
#define NAME_MAX 255
#define RETSTR_MAX 16
#define IDBUF_SIZE 32
#define WIN_WIDTH 425
#define WIN_HEIGHT 500

enum {
    PROCESS_NAME = 0,
    USER,
    CPU,
    ID,
    MEMORY,
    VSIZE,
    TIME,
    IMAGE,
    COLUMNS
};

//struct to hold information on a process
typedef struct procs {
    char *name;
    char *user;
    int utime;
    int stime;
    int pid;
    long vsize;
} proc;

//holds previous process times for calculation cpu %
float prev_proctime[PIDMAX] = {0};
float prev_cputime = 0;


/*  function fscanf_skip()
 *  skips over strings with scanf
 *  parameter: fp, file to scanf on
                n, number of strings to skip
 *  return: void
 */
void fscanf_skip(FILE *fp, int n) {
    for (int i = 0; i < n; i++) {
        fscanf(fp, "%*s");
    }    
}

/*  function get_proc_info()
 *  gets needed information on process from /proc/pid
 *  parameter: pid of process to find
 *  return: 
 */
proc get_proc_info(int pid) {

    char path[PATH_MAX];
    FILE* fp;
    struct stat st;
    struct passwd *pwd;
    proc pr; 

    //get process info from pid
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    if ((fp = fopen(path, "r"))) {
        fscanf(fp, "%d (%[^)])", &pr.pid, pr.name);
        fscanf_skip(fp, 11); 
        fscanf(fp, "%d %d", &pr.utime, &pr.stime);
        fscanf_skip(fp, 7);
        fscanf(fp, "%ld", &pr.vsize); 
        fclose(fp);
    }
    else {
        printf("could not open /stat\n");            
        pr.pid = -1;
        return pr;
    }

    //get user
    if (stat(path, &st)) {
        pr.user = "root";
    }
    else {
        pwd = getpwuid(st.st_uid);
        pr.user = pwd->pw_name;
    }     
    return pr;
}

/*  function get_cputime()
 *  gets the current cpu time from proc
 *  parameter: void
 *  return: total cpu time
 */
int get_cputime() {
    
    FILE *fp;
    char *path = "/proc/stat";
    int cputime = 0, temptime;

    //get cputime from /proc/stat
    if ((fp = fopen(path, "r"))) {
        fscanf(fp, "%*s ");
        while ((fscanf(fp, "%d ", &temptime))) {
            cputime += temptime;
        }
        fclose(fp);
    }
    else {
        printf("could not open /proc/stat\n");            
    }    
    return cputime;
}

/*  function cpu_percent()
 *  gets the percent of cpu use for a given process
 *  parameter: pr, struct containing process information
               cputime, current cpu time
 *  return: a string with the percentage of cpu time used
 */
char *cpu_percent(proc pr, int cputime) {

    float percent, total;
    float prev = prev_proctime[pr.pid];
    float cur = (float)pr.utime + (float)pr.stime;
    char *ret_str = malloc (sizeof (char) * RETSTR_MAX);

    //calculate percent of cpu used by dividing time process used
    //in the last second by total time in the last second
    if (prev_cputime && prev) {
        total = (float)cputime - prev_cputime; 
        percent = 100 * (cur - prev) / total;          
    } 
    sprintf(ret_str, "%.1f", percent);
    prev_proctime[pr.pid] = cur;

    return ret_str;
}

/*  function format_vsize()
 *  used to format the string for memory
 *  parameter: vsize, size of virtual memory for a process
 *  return: a string with the vsize converted to either kb, mb, orgb
 */
char *format_vsize(long vsize) {

    char *ret_str = malloc (sizeof (char) * RETSTR_MAX);
    char *na = "N/A";
    
    //separates vsize into GiB, MiB, or KiB and creates a string with that unit
    if (vsize >= 1000000000) sprintf(ret_str, "%.1f GiB", (float)vsize/1000000000);
    else if (vsize >= 1000000) sprintf(ret_str, "%.1f MiB", (float)vsize/1000000);
    else if (vsize > 0) sprintf(ret_str, "%.1f KiB", (float)vsize/1000);
    else sprintf(ret_str, "%s", na);

    return ret_str;
}

/*  function build_list()
 *  builds list store from pid entries in /proc
 *  parameter: store, pointer to the list store to append to
 *  return: return 0 on success
 */
int build_list(GtkListStore* store) {

    DIR* dir;
    struct dirent* ent;
    proc pr; 
    char idbuf[IDBUF_SIZE];
    char *percent_str, *vsize_str; 
    int pid, cputime = get_cputime();
    
    GError *error = NULL;
    GdkPixbuf* image1 = gdk_pixbuf_new_from_file("icon.png", &error);
    GdkPixbuf* image2 = gdk_pixbuf_new_from_file("icon2.png", &error);
    GdkPixbuf* image;

    GtkTreeIter iter;

    //add each process in /proc to store
    if (!(dir = opendir("/proc"))) {
        perror("can't open /proc");
    }
    else {      
        while((ent = readdir(dir)) != NULL) {
            //skip file names that arent ints
            if (!(pid = atoi(ent->d_name))) continue;
            pr = get_proc_info(pid);
            if (pr.pid == -1) continue;     
            
            //prepare data from get_proc_info for list store
            percent_str = cpu_percent(pr, cputime); 
            vsize_str = format_vsize(pr.vsize); 
            sprintf(idbuf, "%d", pr.pid);
            image = (strcmp(pr.user, "root")) ? image1 : image2;

            //add process info to list store
            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter, PROCESS_NAME, pr.name,
                                USER, pr.user,
                                CPU, percent_str,
                                ID, idbuf,
                                MEMORY, vsize_str,
                                VSIZE, pr.vsize,
                                TIME, pr.utime+pr.stime,
                                IMAGE, image, -1);
            free(percent_str);
            free(vsize_str);
        }
        closedir(dir);           
    }
    prev_cputime = cputime;
    return 0;
}

/*  function build_treeview()
 *  creates columns using list store and adds them to treeview
 *  parameter: treeview
 *  return: void
 */
void build_treeview (GtkWidget *treeview) {
    
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    char *column_names[] = {"Process Name", "User", "CPU", "ID", "MEMORY"};

    //add relevent fields from list store as columns to tree view
    for (int i = PROCESS_NAME; i <= MEMORY; i++) {    
        column = gtk_tree_view_column_new();
        
        //for process add image as well
        if (i == PROCESS_NAME) {
            renderer = gtk_cell_renderer_pixbuf_new();
            gtk_tree_view_column_pack_start(column, renderer, FALSE);
            gtk_tree_view_column_set_attributes(column, renderer, "pixbuf", IMAGE, NULL);           
        }
        renderer = gtk_cell_renderer_text_new();
        
        //allign memory to the right
        if (i == MEMORY) 
            gtk_cell_renderer_set_alignment(renderer, 1, .5);
        
        gtk_tree_view_column_pack_start(column, renderer, FALSE);
        gtk_tree_view_column_set_attributes (column, renderer, "text", i, NULL); 
        gtk_tree_view_column_set_title(column, column_names[i]);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

        //set column to be sortable
        gtk_tree_view_column_set_sort_column_id(column, i);
        gtk_tree_view_column_set_sort_indicator(column, TRUE);  
    }
}

/*  function update_list()
 *  updates the list store, called once a second by timeout
 *  parameter: treeview
 *  return: TRUE to tell timeout to continue
 */
gboolean update_list(GtkWidget* treeview) {
    
    GtkListStore *store; 

    //grabs store from treview and then removes it from tree view
    //then clears store, rebuilds it from /proc, and adds it back to treeview
    //the reason for this is performance as treeview automatically refreshes as 
    //changes are made to store. This way it happens once.
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)));
    g_object_ref(store);
    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), NULL);

    gtk_list_store_clear(store);
    build_list(store);
    
    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(store));
    g_object_unref(store);

    return TRUE;
}

/*  function dispaly()
 *  sets up gtk display
 *  parameter: treeview, gtk treeview  
 *  return: void
 */
void display (GtkWidget *treeview) {
    
    // create the window
    GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "Process List");
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    gtk_widget_set_size_request (window, WIN_WIDTH, WIN_HEIGHT);
    g_signal_connect (window, "delete_event", gtk_main_quit, NULL);
    
    // create a scrolled window
    GtkWidget* scroller = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW (scroller), FALSE);

    // pack the containers
    gtk_container_add (GTK_CONTAINER (scroller), treeview);
    gtk_container_add (GTK_CONTAINER (window), scroller);
    gtk_widget_show_all (window);
}

/*  function sort_func()
 *  used to sort columns 
 *  parameter: model, store
               a, iter pointing to a row
               b, iter pointing to another row
               data, data passed by user which is 
               used to tell which column is being sorted
 *  return: gint, represent order rows a and b should be in
 */
gint sort_func (GtkTreeModel *model,
                      GtkTreeIter *a, GtkTreeIter *b,
                      gpointer data) {
    
    gchar *value1, *value2;
    long m1, m2;
    int order = 0;
    gint col = GPOINTER_TO_INT(data);

    gtk_tree_model_get (model, a, data, &value1, -1);
    gtk_tree_model_get (model, b, data, &value2, -1);
    
    //based on what column is being sorted a different
    //type of comparison is used 
    if (col == PROCESS_NAME || col == USER) 
        order = strcmp(value1, value2);  
    else if (col == CPU)
        order = (atof(value1) > atof(value2)) ? 1 : -1;
    else if (col == ID) {
        order = (atoi(value1) > atoi(value2)) ? 1 : -1;
    }
    //vsize is saved in list store (though not displayed)
    //to make this comparison easier
    else if (col == MEMORY) {
        gtk_tree_model_get (model, a, VSIZE, &m1, -1);
        gtk_tree_model_get (model, b, VSIZE, &m2, -1);        
        order = (m1 > m2) ? 1 : -1;
    }  
    g_free(value1);
    g_free(value2);
    return -order;
}

int main(int argc, char* argv[]) {

    gtk_init (&argc, &argv);
    
    //create list store and populate it from /proc with build_list
    GtkListStore *store = gtk_list_store_new (COLUMNS, G_TYPE_STRING,
                                              G_TYPE_STRING, G_TYPE_STRING,
                                              G_TYPE_STRING, G_TYPE_STRING,
                                              G_TYPE_LONG, G_TYPE_INT, 
                                              GDK_TYPE_PIXBUF);
    GtkTreeSortable *sortable = GTK_TREE_SORTABLE (store);
    for (int i = PROCESS_NAME; i <= MEMORY; i++) {
        gtk_tree_sortable_set_sort_func (sortable, i, sort_func, 
                                         GINT_TO_POINTER (i), NULL);
        gtk_tree_sortable_set_sort_column_id (sortable, i,
                                              GTK_SORT_ASCENDING);        
    }
    if (build_list(store) != 0) {
        printf("Error building list from data\n");
        return 1;
    }    
    
    GtkWidget *treeview = gtk_tree_view_new ();
    build_treeview(treeview);
    gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
    g_object_unref (store);    
    //this calls update_list each second and passes it treeview
    g_timeout_add (1000, (GSourceFunc) update_list, treeview);
    display (treeview);
    
    gtk_main ();

    return 0;
}