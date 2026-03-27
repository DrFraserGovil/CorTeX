# CorTeX: A LaTeX Second Brain

CorTeX is a LaTeX-native knowledge system of individually compiled notes, connected via an internal graph-linkage system. 

## **Core Functionality**

1. **Note-Centric Workflow**

   * CorTeX is built around small, modular units of knowledge (“notes”), similar to systems like Obsidian or Foam. Each note represents a discrete idea and should be lightweight to create and edit.

2. **LaTeX-Native Authoring**

   * Notes are written in LaTeX and treated as structured text
     * Minor extensions to the LaTeX syntax are supported (i.e. wikilinking resolves to ``ref``, simplified typeface manipulation)
   * CorTeX is not intended to replace computational environments (e.g. notebooks).

3. **Per-Note Compilation**

   * Each note compiles independently into a PDF.
   * Compiled outputs are easily viewable outside the editor.

4. **Linking and Referencing**

   * The system supports:

     * Wikilinks (`[[note]]`)
     * Transclusions (embedding content across notes)
     * Native LaTeX referencing (`\ref{}`, `\label{}`, etc.)
   <!-- * These mechanisms should be unified under a consistent internal model, with the Sentinel responsible for resolving and validating links. -->
   * Each file has an *identifier* which can be used as a link target. The *title* of a note is always a valid identifier, but additional identifiers can be provided through the *alias* metadata.
     * Collisions in *alias* data should result in an error message, whilst collisions in *title*-identifiers should lead to a disambiguation page.
5. **System Architecture**
   CorTeX consists of two primary components:

   * **Sentinel**

     * A background process that monitors the workspace for file changes
     * Handles incremental compilation
     * Maintains the dependency graph and metadata index
   * **VSCode Interface**

     * Provides syntax highlighting and editing support
     * Visualizes links, backlinks, and orphaned notes
     * Exposes graph/network views of the knowledge base

6. **Standardized Preamble Management**

   * The Sentinel injects a global preamble into every note at compile time
   * Notes may include a minimal local preamble for overrides
   * Local redefinitions are scoped to the current note only
   * Notes use the following metadata syntax

```
title: ... //required
alias: ... //optional
tags: [...] //optional
---
[local preamble]
--- 
[Note text]
```

7. **Single Source of Truth (SSoT) for Macros**

   * All shared macros and definitions are maintained centrally
   * Per-note macro definitions are discouraged to ensure consistency and maintainability

8. **Failure-Safe Compilation**
   * The system can accept incomplete or malformed links without catastrophic failure 
   * Failed links (`orphans') are be collated and highlighted. 
   * Individual notes which fail to compile are highlighted. We do not attempt to explicitly fix the errors
     * If a file fails to compile, we attempt to extract the `minimal compiling snippet' (MCS), and then insert an error message, containing the full latex error.
     * MCS is generated internally by recursively removing lines from the end of the internally-resolved snippet until the file compiles.


## **Extended Functionality (Secondary Priorities)**

1. **Multi-Format Support and Translation Layer**

   * The system should support multiple input formats (e.g. `.tex`, `.md`, `.tikz`)
   * A translation layer converts supported formats into LaTeX or otherwise makes them transcludable
   <!-- * File types are categorized by level of support (native, convertible, embedded) -->
2. **Form Input Integration**
   * The VS code interface and sentinel should make it easy to establish `structured' metatypes which evaluate to json files that can be viewed as forms in the viewer, but resolve to more complex datatypes in the compiled latex 
  
3. **Daily Notes Integration**

   * The VSCode interface should support automatic generation of daily notes
   * Notes are named and organized by date for quick capture and journaling

4. **Search and Query Engine**

   * A structured query system for filtering and retrieving notes
   * Supports tags, categories, and metadata-based queries

5. **External Reference Integration**

   * Tools for annotating external documents (e.g. papers, PDFs)
   * Integration into a unified bibliography and citation system

6. **Full Document Compilation**

   * Ability to compile a subset or the entirety (or a subset meeting query requirements) of the knowledge base into a single structured document
   * Requires mechanisms for ordering, hierarchy, and inclusion control
