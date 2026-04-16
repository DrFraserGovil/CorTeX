import * as vscode from 'vscode';

//A holder for the PDF data and associated metadata
//Means we can cache in already viewed PDFs and not need to recompute etc
//Can also add in file watchers for open(ed) PDFs, rather than watching *all* PDFs in the archive (i.e. only watch those the user has indicated an interest in)
export class CortexPreview
{
    public Name : string; //the name to put in the tab (derived from metadata, not filename)
    public Source : vscode.Uri;
    public Annotations: Map<string,any> = new Map();
    constructor(
        source : vscode.Uri
    )
    {
        this.Name = "Cortex File"; // placeholder
        this.Source = source;
        console.log("New resource initialised:",this.Name);
    }

    public addAnnotation(data: any[])
    {
        data.forEach(annotation =>{
            if (annotation.id)
            {
                const inferURI = annotation.unsafeUrl.split('#')[0] || annotation.url || annotation.file;
                this.Annotations.set(annotation.id,inferURI);
            }
        })
    }

    public getLink(target: string) : string | undefined
    {
        return this.Annotations.get(target);
    }

    //called when the file is re-read from disk and the cache becomes dirty
    public ClearCache()
    {
        this.Annotations.clear();
    }
}