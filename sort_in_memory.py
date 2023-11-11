import csv
import os

alldomains = []
regexlines = []

class DomainInfo:
    def __init__(self, csv_row):
        # cannot do much with the regex i.e. with '2'
        self.csv_row = csv_row
        self.i = 0
        self._split_domain = self.domain.split('.')
        self._split_domain.reverse()

    def __lt__(self, other):
        maxcnt = min(len(self.rev_domain), len(other.rev_domain))

        if self.i >= maxcnt:
            return len(self.rev_domain) < len(other.rev_domain)

        if len(self.rev_domain[self.i]) < len(other.rev_domain[self.i]):
            return True
        if len(self.rev_domain[self.i]) == len(other.rev_domain[self.i]):
            if self.rev_domain[self.i] < other.rev_domain[self.i]:
                return True
            
        return False

    def compare(self, section):
        self.i = section
        return self

    @property
    def match_strength(self):
        return int(self.csv_row[6])

    @property
    def domain(self):
        return self.csv_row[1]

    @property
    def rev_domain(self):
        return self._split_domain

    @property
    def listname(self):
        return self.csv_row[4]

    @property
    def groupname(self):
        return self.csv_row[5]

    @property
    def domain_segments(self):
        return len(self._split_domain)


def sort_domains(filename):
    global alldomains, max_domain_segments
    # least significant
    #alldomains = sorted(alldomains, key=lambda x: x.split_domain)
    #alldomains = sorted(alldomains, key=lambda x: x.listname)
    #alldomains = sorted(alldomains, key=lambda x: x.groupname)
    # most significant
    #alldomains = sorted(alldomains, key=lambda x: x.glob(2))
    #alldomains = sorted(alldomains, key=lambda x: x.glob(1))
    #alldomains = sorted(alldomains, key=lambda x: x.glob(0))
    for i in reversed(range(max_domain_segments)):
        print("%s: sorting index=%d" % (os.path.basename(filename), i))
        alldomains = sorted(alldomains, key=lambda x: x.compare(i))
    #alldomains = sorted(alldomains, key=lambda x: len(x.rev_domain))
    #alldomains = sorted(alldomains, key=lambda x: x.match_strength,
            #reverse=True)

def read_csv(filename):
    global alldomains, regexlines, max_domain_segments

    current_max = 0
    rows_read = 0
    with (open(filename, 'r', newline='') as csvinfile):
        pfb_py_reader = csv.reader(csvinfile, delimiter=',')
        for rows_read, row in enumerate(pfb_py_reader, start=1):
            if not (len(row) == 7 or len(row) == 6):
                print("NOTE: Ignoring row with %s columns" % len(row), row)
                continue

            # 7th column is 0, 1 or 2
            match_strength = int(row[6]) if len(row) == 7 else 0
            if not (match_strength >= 0 or match_strength <= 2):
                print("NOTE: Ignoring row with unrecognized value for 7th column: %d (expected >= 0 and <= 2)" % match_strength)
                continue

            if match_strength < 2:
                alldomains.append(DomainInfo(row))
                current_max = max(current_max, alldomains[-1].domain_segments)
            else:
                # do not modify the regex rows
                regexlines.append(row)

    print("read %d rows from csv" % rows_read)
    max_domain_segments = current_max
    return rows_read > 0

def run_in_memory(filenames):
    global alldomains, regexlines

    for f in filenames:
        alldomains = []
        regexlines = []
        if not read_csv(f):
            print("Zero rows read from file:", f)
            fsorted = f + ".sorted"
            print("write to:", fsorted)
            with open(fsorted, 'w', newline='') as emptyf:
                emptyf.write('\n')
            return

        sort_domains(f)

        regexlines = sorted(regexlines)
        #alldomains = sorted(alldomains, key=lambda x: x.csv_row)

        fsorted = f + ".sorted"
        print("write to:", fsorted)
        with open(fsorted, 'w', newline='') as csvoutfile:
            pfb_py_writer = csv.writer(csvoutfile, delimiter=',',
                    quotechar="'", lineterminator='\n')
            for r in regexlines:
                pfb_py_writer.writerow(r)
            for di in alldomains:
                pfb_py_writer.writerow(di.csv_row)

def main(filename):
    from timeit import default_timer as timer
    start = timer()
    run_in_memory(filename)
    end = timer()
    print("total time:", end - start)

import sys
if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("requires filename to process")
        sys.exit(1)
    main(sys.argv[1:])
