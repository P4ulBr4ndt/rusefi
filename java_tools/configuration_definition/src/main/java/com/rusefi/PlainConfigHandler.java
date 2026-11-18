package com.rusefi;

import com.rusefi.output.PlainTsProjectConsumer;

import java.io.IOException;
import java.util.List;

/**
 * used to prepare secondary pages in TS .ini file
 */
class PlainConfigHandler {
    ReaderStateImpl readerState = new ReaderStateImpl();
    PlainTsProjectConsumer tsProjectConsumer = new PlainTsProjectConsumer(readerState);

    public PlainConfigHandler(String definitionInputFile, int pageIndex, List<String> softPrepends) {
        this(definitionInputFile, RootHolder.ROOT + "../firmware/controllers/generated/page_" + pageIndex + "_generated.h", softPrepends);
    }

    /**
     * @param cHeaderDestination relative to META_OUTPUT_ROOT_FOLDER, so board-specific pages land in the board's generated folder
     */
    public PlainConfigHandler(String definitionInputFile, String cHeaderDestination, List<String> softPrepends) {
        readerState.setDefinitionInputFile(definitionInputFile);
        for (String soft : softPrepends)
            readerState.addSoftPrepend(soft);
        readerState.addPrepend("integration/rusefi_config_shared.txt");
        readerState.addPrepend("integration/ts_protocol.txt");

        readerState.addDestination(tsProjectConsumer);
        readerState.setWithC_Defines(false);
        readerState.addCHeaderDestination(cHeaderDestination);
    }

    public void doJob() throws IOException {
        readerState.doJob();
    }
}
