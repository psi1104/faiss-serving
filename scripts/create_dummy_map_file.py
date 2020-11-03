from create_test_faiss_embedding import nb

with open('map-file.txt', 'w') as f:
    for i in range(nb):
        print(f"string_{i}", file=f)
