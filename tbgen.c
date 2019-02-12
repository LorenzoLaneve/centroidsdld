//
//  Test Bench Generator
//  Project of Digital Logic Design
//  Politecnico di Milano, A.Y. 2018/19
//
//  (c) 2019 Lorenzo Laneve
//

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_CASE_COUNT 20

//#define random_byte() (random() % 256)
#define random_in(x,y) ((x) + random() % ((y) - (x))) // x incluso, y escluso
#define random_out_of(x,y) ((((y)+1) + random() % (256 - (y) + (x))) % 256) // x incluso, y escluso

#define random_bit() (random() % 2)

#define binary_format "%c%c%c%c%c%c%c%c"
#define to_bin(byte)  \
    ((byte) & 0x80 ? '1' : '0'), \
    ((byte) & 0x40 ? '1' : '0'), \
    ((byte) & 0x20 ? '1' : '0'), \
    ((byte) & 0x10 ? '1' : '0'), \
    ((byte) & 0x08 ? '1' : '0'), \
    ((byte) & 0x04 ? '1' : '0'), \
    ((byte) & 0x02 ? '1' : '0'), \
    ((byte) & 0x01 ? '1' : '0')

struct {
    int random_cases;
    
    char arch_name[50];
    char directory[256];
} opts;

typedef struct {
    unsigned char X;
    unsigned char Y;
} point;

typedef struct {
    point points[8];
    point main_point;
    
    unsigned char input_mask;
    
    unsigned char output_mask;
} test_bench;


// RISOLUZIONE

unsigned taxicab_distance(point *p1, point *p2) {
    return (unsigned)(abs((int)p1->X - (int)p2->X) + abs((int)p1->Y - (int)p2->Y));
}

void solve(test_bench *tb) {
    unsigned min_distance = ~0U;
    tb->output_mask = 0;
    
    point *main_point = &tb->main_point;
    for (int i = 0; i < 8; i++) {
        if (tb->input_mask & (1 << i)) {
            point *p = &tb->points[i];
            
            unsigned distance = taxicab_distance(main_point, p);
            if (distance <= min_distance) {
                if (distance < min_distance) {
                    min_distance = distance;
                    tb->output_mask = 0;
                }
                
                tb->output_mask |= 1 << i;
            }
        }
    }
}

// GENERAZIONE TEST

int *rand_permutation() { // algoritmo di Fisher-Yates, permutazione random di [0...7]
    int *res = (int *)malloc(sizeof(int)*8);
    
    for (int i = 0; i < 8; i++) {
        res[i] = i;
    }
    
    for (int i = 7; i > 0; i--) {
        int j = random() % (i+1);
        
        int t = res[j];
        res[j] = res[i];
        res[i] = t;
    }
    
    return res;
}

unsigned char random_byte(int ones) {
    int *p = rand_permutation();
    
    unsigned char res = 0;
    
    int i;
    for (i = 0; i < ones; i++) {
        res |= 1 << p[i];
    }
    
    free(p);
    return res;
}

test_bench *new_ordinary_tb(int nearest_points) {
    test_bench *this_ = (test_bench *)malloc(sizeof(test_bench));
    this_->input_mask = random_byte(random_in(0, 9));
    
    int min_distance = random_in(0, 50);

    point *main_point = &this_->main_point;
    main_point->X = random_in(min_distance, 256 - min_distance);
    main_point->Y = random_in(min_distance, 256 - min_distance);
    
    int *p = rand_permutation();
    
    int i;
    for (i = 0; i < nearest_points; i++) {
        point *nearp = &this_->points[p[i]];
        
        nearp->X = main_point->X;
        nearp->Y = main_point->Y;
        
        int offsetX = random_in(0, min_distance+1);
        int offsetY = min_distance - offsetX;
        
        nearp->X += random_bit() ? offsetX : -offsetX;
        nearp->Y += random_bit() ? offsetY : -offsetY;
        
        assert(taxicab_distance(nearp, main_point) == min_distance && "Errore: generazione del punto vicino errata.");
    }
    
    for (; i < 8; i++) {
        point *farp = &this_->points[p[i]];
        
        if (random_bit()) { // X
            farp->X = random_out_of(main_point->X-1, main_point->X);
            
            int offset = min_distance - abs(farp->X - main_point->X);
            farp->Y = random_out_of(main_point->Y-1 - offset, main_point->Y + offset);
        } else { // Y
            farp->Y = random_out_of(main_point->Y-1, main_point->Y);
            
            int offset = min_distance - abs(farp->Y - main_point->Y);
            farp->X = random_out_of(main_point->X-1 - offset, main_point->X + offset);
        }
        
        assert(taxicab_distance(farp, main_point) > min_distance && "Errore: generazione del punto lontano errata.");
    }
    
    solve(this_);
    
    free(p);
    return this_;
}

// GENERATORE VHDL

void write_ram_value(FILE *fp, int addr, int value) {
    fprintf(fp, "\t\t%d => std_logic_vector(to_unsigned(%d, 8)),\n", addr, value);
}

void generate_vhdl(FILE *fp, test_bench *tb, int testid) {
    assert(fp && "Nessun file aperto.");
    assert(tb && "Nessun test bench specificato.");
    
    fprintf(fp, "--\n");
    fprintf(fp, "-- This is a randomly auto-generated test bench.\n");
    fprintf(fp, "--\n\n");
    fprintf(fp, "library ieee;\n");
    fprintf(fp, "use ieee.std_logic_1164.all;\n");
    fprintf(fp, "use ieee.numeric_std.all;\n");
    fprintf(fp, "use ieee.std_logic_unsigned.all;\n\n");
    
    fprintf(fp, "entity autogen_test_bench_%d is\n", testid);
    fprintf(fp, "end autogen_test_bench_%d;\n\n", testid);
    
    fprintf(fp, "architecture %s of autogen_test_bench_%d is\n", opts.arch_name, testid);
    fprintf(fp, "\tconstant c_CLOCK_PERIOD        : time := 100 ns;\n");
    fprintf(fp, "\tsignal   tb_done               : std_logic;\n");
    fprintf(fp, "\tsignal   mem_address           : std_logic_vector (15 downto 0) := (others => '0');\n");
    fprintf(fp, "\tsignal   tb_rst                : std_logic := '0';\n");
    fprintf(fp, "\tsignal   tb_start              : std_logic := '0';\n");
    fprintf(fp, "\tsignal   tb_clk                : std_logic := '0';\n");
    fprintf(fp, "\tsignal   mem_o_data,mem_i_data : std_logic_vector (7 downto 0);\n");
    fprintf(fp, "\tsignal   enable_wire           : std_logic;\n");
    fprintf(fp, "\tsignal   mem_we                : std_logic;\n\n");
    
    fprintf(fp, "\ttype ram_type is array (65535 downto 0) of std_logic_vector(7 downto 0);\n");
    fprintf(fp, "\tsignal RAM: ram_type := (\n");
    
    fprintf(fp, "\t\t0 => \"" binary_format "\",\n", to_bin(tb->input_mask));
    
    int k = 1;
    for (int i = 0; i < 8; i++) {
        write_ram_value(fp, k++, tb->points[i].X);
        write_ram_value(fp, k++, tb->points[i].Y);
    }
    
    write_ram_value(fp, 17, tb->main_point.X);
    write_ram_value(fp, 18, tb->main_point.Y);
    
    fprintf(fp, "\t\tothers => (others =>'0')\n");
    fprintf(fp, "\t);\n\n");
    
    fprintf(fp, "\tconstant EXPECTED_OUTPUT : std_logic_vector(7 downto 0) := \"" binary_format "\";\n\n", to_bin(tb->output_mask));
    
    fprintf(fp, "\tcomponent project_reti_logiche is\n");
    fprintf(fp, "\t\tport (\n");
    fprintf(fp, "\t\t\ti_clk         : in  std_logic;\n");
    fprintf(fp, "\t\t\ti_start       : in  std_logic;\n");
    fprintf(fp, "\t\t\ti_rst         : in  std_logic;\n");
    fprintf(fp, "\t\t\ti_data        : in  std_logic_vector(7 downto 0);\n");
    fprintf(fp, "\t\t\to_address     : out std_logic_vector(15 downto 0);\n");
    fprintf(fp, "\t\t\to_done        : out std_logic;\n");
    fprintf(fp, "\t\t\to_en          : out std_logic;\n");
    fprintf(fp, "\t\t\to_we          : out std_logic;\n");
    fprintf(fp, "\t\t\to_data        : out std_logic_vector (7 downto 0)\n");
    fprintf(fp, "\t);\n");
    fprintf(fp, "\tend component project_reti_logiche;\n\n");
    
    fprintf(fp, "begin\n");
    fprintf(fp, "UUT:\n\tproject_reti_logiche port map (\n");
    fprintf(fp, "\t\ti_clk          => tb_clk,\n");
    fprintf(fp, "\t\ti_start        => tb_start,\n");
    fprintf(fp, "\t\ti_rst          => tb_rst,\n");
    fprintf(fp, "\t\ti_data         => mem_o_data,\n");
    fprintf(fp, "\t\to_address      => mem_address,\n");
    fprintf(fp, "\t\to_done         => tb_done,\n");
    fprintf(fp, "\t\to_en           => enable_wire,\n");
    fprintf(fp, "\t\to_we           => mem_we,\n");
    fprintf(fp, "\t\to_data         => mem_i_data\n");
    fprintf(fp, "\t);\n\n");
    
    fprintf(fp, "p_CLK_GEN:\n\tprocess is\n");
    fprintf(fp, "\tbegin\n");
    fprintf(fp, "\t\twait for c_CLOCK_PERIOD/2;\n");
    fprintf(fp, "\t\ttb_clk <= not tb_clk;\n");
    fprintf(fp, "\tend process p_CLK_GEN;\n");
    
    fprintf(fp, "MEM:\n\tprocess(tb_clk)\n");
    fprintf(fp, "\tbegin\n");
    fprintf(fp, "\t\tif tb_clk'event and tb_clk = '1' then\n");
    fprintf(fp, "\t\t\tif enable_wire = '1' then\n");
    fprintf(fp, "\t\t\t\tif mem_we = '1' then\n");
    fprintf(fp, "\t\t\t\t\tRAM(conv_integer(mem_address)) <= mem_i_data;\n");
    fprintf(fp, "\t\t\t\t\tmem_o_data                     <= mem_i_data after 2 ns;\n");
    fprintf(fp, "\t\t\t\telse\n");
    fprintf(fp, "\t\t\t\t\tmem_o_data <= RAM(conv_integer(mem_address)) after 2 ns;\n");
    fprintf(fp, "\t\t\t\tend if;\n");
    fprintf(fp, "\t\t\tend if;\n");
    fprintf(fp, "\t\tend if;\n");
    fprintf(fp, "\tend process;\n");
    
    fprintf(fp, "test:\n\tprocess is\n");
    fprintf(fp, "\tbegin\n");
    fprintf(fp, "\t\twait for 100 ns;\n");
    fprintf(fp, "\t\twait for c_CLOCK_PERIOD;\n");
    fprintf(fp, "\t\ttb_rst <= '1';\n");
    fprintf(fp, "\t\twait for c_CLOCK_PERIOD;\n");
    fprintf(fp, "\t\ttb_rst <= '0';\n");
    fprintf(fp, "\t\twait for c_CLOCK_PERIOD;\n");
    fprintf(fp, "\t\ttb_start <= '1';\n");
    fprintf(fp, "\t\twait for c_CLOCK_PERIOD;\n");
    fprintf(fp, "\t\twait until tb_done = '1';\n");
    fprintf(fp, "\t\twait for c_CLOCK_PERIOD;\n");
    fprintf(fp, "\t\ttb_start <= '0';\n");
    fprintf(fp, "\t\twait until tb_done = '0';\n");
    
    fprintf(fp, "\t\tassert RAM(19) = EXPECTED_OUTPUT report \"TEST FALLITO\" severity failure;\n");
    
    fprintf(fp, "\t\tassert false report \"TEST #%d OK\" severity failure;\n", testid);
    fprintf(fp, "\tend process test;\n\n");
    
    fprintf(fp, "end %s;\n", opts.arch_name);
}

int main(int argc, const char *argv[]) {
    
    opts.random_cases = DEFAULT_CASE_COUNT;
    strcpy(opts.arch_name, "bhv");
    strcpy(opts.directory, ".");
    
    int defaultdir = 1;
    
    int x;
    char str[50];
    
    for (int i = 1; i < argc; i++) {
        if (sscanf(argv[i], "--random-cases=%d", &x) && x >= 0) {
            opts.random_cases = x;
        } else if (sscanf(argv[i], "--arch-name=%s", str)) {
            strcpy(opts.arch_name, str);
        } else if (!strcmp(argv[i], "--help")) {
            printf("Usage: %s [opts] [<dir>]\n\n", argv[0]);
            printf("Generates a number of random VHDL test benches and writes them into the specified directory. The directory must exist. If no directory is specified, then the current work directory will be assumed.\n\n");
            
            printf("opts:\n");
            printf(" --arch-name=T\t\tthe name of the architecture for the test benches (NO QUOTES). This must match with the architecture of the component as specified in your VHDL design file. (default 'bhv')\n");
            printf(" --random-cases=X\t\texactly X random cases will be generated.\n\n");
            exit(0);
        } else if (defaultdir) {
            strcpy(opts.directory, argv[i]);
            defaultdir = 0;
        } else {
            fprintf(stderr, "unrecognized option: %s. Use --help for options\n\n", argv[i]);
            exit(1);
        }
    }
    
    srandom((unsigned)time(0));
    
    char filename[50];
    for (int i = 1; i <= opts.random_cases; i++) {
        sprintf(filename, "%s/test%d.vhd", opts.directory, i);
        FILE *fp = fopen(filename, "w");
        if (!fp) {
            fprintf(stderr, "error: could not open file. Please make sure that the target directory exists.\n\n");
            exit(1);
        }
        
        test_bench *tb = new_ordinary_tb(random_in(1, 9));
        generate_vhdl(fp, tb, i);
        
        free(tb);
        fclose(fp);
    }
    
    return 0;
}

