
Let's start with the fundamentals.

Applications want to store data permanently, some examples:
- Game world data
- Text files for configs, source code, UI layout
- Videos/audio to play
- Images to render, logos, screenshots
- The application code, plugins containing code that are user has downloaded

A person wants to store
- Videos, images, games, text files, documents
- Code projects, cad files, assets, binaries

The OS needs to provide an API to create and modify permanent data.

The data is created by the user and each blob of data needs to be named. If an app or user wants to
open an image or code library then a name is better than an object ID. A name can be the same on every system.

The data blobs can have unique ID but then also a name. OR the name can be that unique ID since we can't have two blobs with the same name.

Data is rarely alone. An application is distributed with a group of blobs. This means you often want to move, copy, and delete groups of blobs.
A flat naming system for all blobs is therefore not viable. A hierarchy where each blob has one parent is sensible.





What about the specifics of the API. On creation to you specify the reserved size of the blob and that's it, no growth? Or
can it grow as you write data to it.

For efficiency multiple apps can READ from the same blob.
For stability multiple apps cannot WRITE to the same blob.
For convenience multiple apps can APPEND to the same blob, no arbitrary writes within the blob. Useful for log and performance data.




