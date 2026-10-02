




void render_disks_menu() {
    // Render disk menu where you pick which disk that will be formatted with .img file

    chosen_disk = NULL


    

    for disk in disks {

        render_text(disk.name)

        if mouse_click() {
            chosen_disk = disk
        }
    }

    if chosen_disk {
        
        diskHandle = disk_open(chosen_disk)

        disk_write(diskHandle, 0, fileSize, fileData)

        // reformat GPT based on disk size (copy GPT header to second GPT header at the end of the disk)
        reformat_gpt(diskHandle)

    }

}



