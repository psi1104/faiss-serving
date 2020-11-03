"""
Tutorial Codes from https://github.com/facebookresearch/faiss/wiki/Getting-started
"""


# Getting some data

import numpy as np
d = 64                           # dimension
nb = 100000                      # database size
nq = 10000                       # nb of queries
np.random.seed(1234)             # make reproducible

# Branching not to execute below codes during importing this scripts from create_dummy_map_file
if __name__ == "__main__":

    xb = np.random.random((nb, d)).astype('float32')
    xb[:, 0] += np.arange(nb) / 1000.
    xq = np.random.random((nq, d)).astype('float32')
    xq[:, 0] += np.arange(nq) / 1000.


    # Building an index and adding the vectors to it
    import faiss                   # make faiss available
    index = faiss.IndexFlatL2(d)   # build the index
    print(index.is_trained)
    index.add(xb)                  # add vectors to the index
    print(index.ntotal)

    index = faiss.IndexFlatL2(d)   # build the index
    print(index.is_trained)
    index.add(xb)                  # add vectors to the index
    print(index.ntotal)

    faiss.write_index(index, './test.faiss')
