import * as vscode from 'vscode';

//A holder for the PDF data and associated metadata
//Means we can cache in already viewed PDFs and not need to recompute etc
//Can also add in file watchers for open(ed) PDFs, rather than watching *all* PDFs in the archive (i.e. only watch those the user has indicated an interest in)
export class CortexPreview
{
    public Name : string; //the name to put in the tab (derived from metadata, not filename)

    constructor(
        source : vscode.Uri
    )
    {
        this.Name = source.toString(); // placeholder
    }

}