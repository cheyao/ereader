#!/bin/env python3
from bs4 import BeautifulSoup
import requests
import sys

def get_page(url: str):
    headers = {'user-agent': 'Mozilla/5.0 (X11; Linux x86_64; rv:160.0) Gecko/20100101 Firefox/160.0'}
    response = requests.get(url, headers=headers)
    if response.status_code != 200:
        return None
    return response.content

def get_index():
    index_page = get_page("https://wanderinginn.com/table-of-contents/")

    if index_page == None:
        return []

    index_soup = BeautifulSoup(index_page, features="lxml")
    toc = index_soup.find(id="table-of-contents")

    if toc == None:
        return []

    chapters = toc.find_all(class_="chapter-entry")

    if chapters == None:
        return []

    links = []
    for chapter in chapters:
        a = chapter.find('a')

        if a == None:
            return None

        links += [a.get('href')]

    return links

def index_containing_substring(the_list, substring):
    for i, s in enumerate(the_list):
        if substring in s:
              return i
    return -1

def get_chapter(url):
    text = ""

    chapter_page = get_page(url)

    if chapter_page == None:
        return []

    chapter = BeautifulSoup(chapter_page, features="lxml")
    title_elem = chapter.title
    if title_elem == None:
        return []
    title = ' - '.join((title_elem.text.split(' - ')[:-1]))

    text += '\n' + title + '\n'

    main_text = chapter.find('article', class_="twi-article")
    if main_text == None:
        return []

    main_text = main_text.find_all('p')[:-1]
    if main_text == None:
        return []

    for paragraph in main_text:
        ptext = paragraph.text

        if not ptext or not ptext.strip():
            continue

        text += '\n\n' + ptext

    text = text.replace('“', '"')
    text = text.replace('”', '"')
    text = text.replace('’', '\'')
    text = text.replace('—', '\x1f')
    text = text.replace('…', '...')

    return [title, text]

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <start_chapter> <end_chapter>")
        exit()

    index = get_index()
    
    if index == None:
        print("Error! Failed to get index of chapters")
        exit()

    start = sys.argv[1].replace('.', '-')
    end = sys.argv[2].replace('.', '-')

    start_index = index_containing_substring(index, start)
    end_index = index_containing_substring(index, end)

    index = index[start_index:end_index+1]

    i = 1
    for chapter in index:
        text = get_chapter(chapter)

        with open(f"{i:04d}" + '_' + text[0].replace(' ', '_') + '.txt', "w") as file:
            file.write(text[1])

        i = i + 1

