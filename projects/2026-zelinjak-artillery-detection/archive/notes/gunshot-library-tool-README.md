# Gunshot-library-tool: Parquet visualiser

This application allows you to view and edit Parquet libraries, merge selected libraries, and visualize the corresponding audio data from files. The application is built using the **Streamlit** framework.

## Requirements

- Python 3.7 or newer
- Required packages specified in `requirements.txt`

## Installation

1. **Clone the repository**

   Clone the source code of the application to your local machine.

   ```bash
   git clone https://gitlab.fel.cvut.cz/maxamart/gunshot-library-tool.git
   ```
2. **Install the required packages**

   Install the necessary Python packages listed in the `requirements.txt` file:

   ```bash
   pip install -r requirements.txt
   ```

## Running the Application

To run the application, use the following command:

```bash
streamlit run main.py
```

Once the app is running, you will see a local URL where you can access the application in your browser.

## Usage

1. **Upload Parquet Libraries**

   Use the file uploader to upload one or more Parquet libraries for browsing, editing, and merging.

2. **Set Prefix and Audio File Path**

   You can set a prefix for the filenames and browse for the folder where the audio files are located. This is useful for managing audio data in the correct paths when visualizing the recordings.

3. **Data Visualization**

   After selecting a library and expanding a record, use the chart icon to visualize the corresponding audio data. The application can plot either a single audio file or multiple files in a combined graph, depending on how many are available for the record.

4. **Merge Libraries**

   Select multiple libraries and merge them. You can download the merged data in Parquet format.

## Notes

- The application works with Parquet libraries, where each library should have corresponding audio data in `.wav` format.
