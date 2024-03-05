# Comments

100 zones - sudo ./db_bench --db=/mnt/f2fs/db0 --wal_dir=/mnt/f2fs/wal0 --num=30000000 --compression_type=none --value_size=1000 --key_size=16 --use_direct_io_for_flush_and_compaction --num_levels=5 --max_bytes_for_level_multiplier=2 --max_background_jobs=8 --target_file_size_base=282329088 --write_buffer_size=564658176 --histogram --benchmarks=overwrite,overwrite,levelstats --seed=42 --use_existing_db
